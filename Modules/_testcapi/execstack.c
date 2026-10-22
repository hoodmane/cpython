/* Tests for the PyUnstable_ExecStack API.
 *
 * To exercise the API the way greenlet does, this file implements a minimal
 * stackful coroutine ("Fiber") that shares one C stack region between all
 * fibers of a thread, the way greenlet shares the thread's own stack:
 *
 *  - Every fiber runs on the same region. While a fiber runs, the C stack of
 *    every suspended fiber has been copied to the heap, and the region is
 *    filled with a poison byte before the incoming fiber's slice is copied
 *    back in. So the entry frames that the eval loop keeps on the C stack of
 *    a suspended fiber are guaranteed to be garbage: anything in the
 *    interpreter that follows them (f_back, GC traversal, ...) would crash
 *    if the ExecStack API did not unlink them.
 *
 *  - The switch itself is done by a trampoline running on a separate scratch
 *    stack (so it can overwrite the region freely); the register save and
 *    restore is left to ucontext, and the stack pointer of a suspended
 *    fiber is read out of its ucontext_t. That is the only platform specific
 *    part, so this is only built for x86-64 glibc.
 *
 *  - The split Suspend()/Resume() protocol is therefore not just exercised
 *    but required: Suspend() runs before the outgoing C stack is destroyed
 *    and Resume() after the incoming one has been restored.
 */

#include "parts.h"
#include "util.h"

#include "frameobject.h"

#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
#  define HAVE_FIBER_TEST 1
#  include <ucontext.h>
#  include <sys/mman.h>
#endif

#ifdef HAVE_FIBER_TEST

// The region shared by all fibers of a thread, and the scratch stack the
// trampoline runs on. Both are mapped lazily per thread and never unmapped
// (this is a test).
#define REGION_SIZE (1024 * 1024)
#define SCRATCH_SIZE (64 * 1024)
// Bytes below the saved stack pointer that we save and restore too: the
// System V red zone, plus room for setcontext() to push the return address.
#define SP_SLACK 256
#define POISON 0xAB

typedef struct FiberObject {
    PyObject_HEAD
    PyUnstable_ExecStack *stack;
    PyThreadState *tstate;              // thread we run on, once started
    struct FiberObject *parent;         // who switched into us; NULL = root
    ucontext_t ctx;                     // registers while suspended
    char *saved;                        // heap copy of [sp - SP_SLACK, end)
    size_t saved_size;
    size_t saved_capacity;
    PyObject *run;
    PyObject *transfer;   // value passed across a switch; NULL + error = throw
    int started;
    int finished;
} FiberObject;

// Per-thread switching state. NULL fiber means "the root", i.e. the thread's
// own C stack, which is outside the region and so never overwritten.
static _Thread_local char *region = NULL;
static _Thread_local char *scratch = NULL;
static _Thread_local FiberObject *current = NULL;
static _Thread_local FiberObject *switch_from = NULL;
static _Thread_local FiberObject *switch_to = NULL;
static _Thread_local ucontext_t root_ctx;
static _Thread_local ucontext_t trampoline_ctx;
static _Thread_local PyUnstable_ExecStack *root_exec_stack = NULL;

static inline char *
region_end(void)
{
    return region + REGION_SIZE;
}

static int
ensure_region(void)
{
    if (region != NULL) {
        return 0;
    }
    region = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        region = NULL;
        PyErr_NoMemory();
        return -1;
    }
    scratch = mmap(NULL, SCRATCH_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (scratch == MAP_FAILED) {
        scratch = NULL;
        PyErr_NoMemory();
        return -1;
    }
    return 0;
}

static inline char *
ctx_sp(const ucontext_t *ctx)
{
    return (char *)ctx->uc_mcontext.gregs[REG_RSP];
}

// Where a fiber's live slice of the region starts, given its saved context.
static inline char *
slice_start(const FiberObject *fiber)
{
    char *sp = ctx_sp(&fiber->ctx) - SP_SLACK;
    assert(sp >= region && sp < region_end());
    return sp;
}

/* The trampoline: runs on the scratch stack, so it may clobber the region. */
static void
trampoline(void)
{
    FiberObject *from = switch_from;
    FiberObject *to = switch_to;
    PyThreadState *tstate = PyThreadState_Get();

    if (from != NULL) {
        // Spill the outgoing fiber's stack. The capacity was reserved before
        // we got here, so this cannot fail.
        char *start = slice_start(from);
        from->saved_size = (size_t)(region_end() - start);
        if (from->saved_size > from->saved_capacity) {
            Py_FatalError("fiber: saved stack capacity too small");
        }
        memcpy(from->saved, start, from->saved_size);
    }

    // Make sure nothing can rely on a suspended fiber's C stack.
    memset(region, POISON, REGION_SIZE);

    if (to == NULL) {
        PyUnstable_ThreadState_ResetStackProtection(tstate);
        setcontext(&root_ctx);
    }
    else {
        if (PyUnstable_ThreadState_SetStackProtection(tstate, region,
                                                      REGION_SIZE) < 0) {
            Py_FatalError("fiber: SetStackProtection failed");
        }
        if (to->started) {
            memcpy(slice_start(to), to->saved, to->saved_size);
            to->saved_size = 0;
        }
        else {
            // First switch into `to`: start it at the top of the region,
            // right where everybody else's frames used to be.
            to->started = 1;
            getcontext(&to->ctx);
            to->ctx.uc_stack.ss_sp = region;
            to->ctx.uc_stack.ss_size = REGION_SIZE;
            to->ctx.uc_link = NULL;
            extern void fiber_entry(void);
            makecontext(&to->ctx, fiber_entry, 0);
        }
        setcontext(&to->ctx);
    }
    Py_FatalError("fiber: setcontext returned");
}

/* Transfer control from the current fiber (or the root) to `to` (or the
 * root). Returns -1 with an exception set, and nothing changed, if the
 * execution stack could not be suspended. Otherwise returns 0 in the
 * original fiber once something switches back to it, with its execution
 * stack resumed. */
static int
jump(FiberObject *to)
{
    PyThreadState *tstate = PyThreadState_Get();
    FiberObject *from = current;

    if (from != NULL) {
        // Reserve room for our stack slice now, while allocation is still
        // allowed. `marker` is above the stack pointer swapcontext() will
        // record, so this over-estimates.
        volatile char marker;
        size_t need = (size_t)(region_end() - (char *)&marker) + SP_SLACK + 4096;
        if (need > from->saved_capacity) {
            char *buf = PyMem_RawRealloc(from->saved, need);
            if (buf == NULL) {
                PyErr_NoMemory();
                return -1;
            }
            from->saved = buf;
            from->saved_capacity = need;
        }
    }

    // Save the Python side of our stack while our C stack is still intact.
    PyUnstable_ExecStack *mine = PyUnstable_ExecStack_Suspend(tstate);
    if (mine == NULL) {
        return -1;
    }
    if (from == NULL) {
        root_exec_stack = mine;
    }

    switch_from = from;
    switch_to = to;
    current = to;
    getcontext(&trampoline_ctx);
    trampoline_ctx.uc_stack.ss_sp = scratch;
    trampoline_ctx.uc_stack.ss_size = SCRATCH_SIZE;
    trampoline_ctx.uc_link = NULL;
    makecontext(&trampoline_ctx, trampoline, 0);
    swapcontext(from ? &from->ctx : &root_ctx, &trampoline_ctx);

    // Somebody switched back to us. Our C stack has been restored, so locals
    // are valid again, but follow greenlet's discipline and reload from
    // thread-local state anyway. `current` was set to us by the switcher.
    tstate = PyThreadState_Get();
    mine = current ? current->stack : root_exec_stack;
    if (PyUnstable_ExecStack_Resume(tstate, mine) < 0) {
        Py_FatalError("fiber: PyUnstable_ExecStack_Resume failed after switch");
    }
    return 0;
}

// First function on a new fiber's stack. Never returns.
void
fiber_entry(void)
{
    FiberObject *self = current;
    PyThreadState *tstate = PyThreadState_Get();
    if (PyUnstable_ExecStack_Resume(tstate, self->stack) < 0) {
        Py_FatalError("fiber: PyUnstable_ExecStack_Resume failed in entry");
    }

    PyObject *arg = self->transfer;
    self->transfer = NULL;
    PyObject *result;
    if (arg == NULL) {
        // Thrown into before it ever ran; propagate the exception.
        assert(PyErr_Occurred());
        result = NULL;
    }
    else {
        result = PyObject_CallFunctionObjArgs(self->run, (PyObject *)self,
                                              arg, NULL);
        Py_DECREF(arg);
    }
    self->transfer = result;
    self->finished = 1;
    if (jump(self->parent) < 0) {
        Py_FatalError("fiber: cannot leave a finished fiber");
    }
    Py_FatalError("fiber: finished fiber was resumed");
}

static PyObject *
fiber_switch_impl(FiberObject *self, PyObject *value)
{
    if (self->finished) {
        Py_XDECREF(value);
        PyErr_Clear();
        PyErr_SetString(PyExc_ValueError, "fiber has finished");
        return NULL;
    }
    if (self == current) {
        Py_XDECREF(value);
        PyErr_Clear();
        PyErr_SetString(PyExc_ValueError, "fiber is already running");
        return NULL;
    }
    PyThreadState *tstate = PyThreadState_Get();
    if (self->started && self->tstate != tstate) {
        Py_XDECREF(value);
        PyErr_Clear();
        PyErr_SetString(PyExc_ValueError,
                        "cannot switch to a fiber of another thread");
        return NULL;
    }
    if (ensure_region() < 0) {
        Py_XDECREF(value);
        return NULL;
    }
    assert(self->transfer == NULL);
    self->transfer = value;
    self->tstate = tstate;
    self->parent = current;
    if (jump(self) < 0) {
        self->transfer = NULL;
        Py_XDECREF(value);
        return NULL;
    }
    // Back from the fiber: it finished or switched back to us.
    PyObject *result = self->transfer;
    self->transfer = NULL;
    assert((result == NULL) == (PyErr_Occurred() != NULL));
    return result;
}

// switch() and switch_back() are METH_FASTCALL so that the specializing
// interpreter and the JIT tracer treat them like any other C method: a
// fiber can then be suspended from inside JIT code, which is what exercises
// the JIT fields of the thread state.
static PyObject *
fiber_switch(PyObject *op, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs > 1) {
        PyErr_SetString(PyExc_TypeError, "switch() takes at most 1 argument");
        return NULL;
    }
    PyObject *value = nargs ? args[0] : Py_None;
    return fiber_switch_impl((FiberObject *)op, Py_NewRef(value));
}

static PyObject *
fiber_throw(PyObject *op, PyObject *exc)
{
    PyErr_SetObject(PyExceptionInstance_Check(exc)
                    ? (PyObject *)Py_TYPE(exc) : exc, exc);
    return fiber_switch_impl((FiberObject *)op, NULL);
}

// Called from inside the fiber: suspend and give `value` to whoever
// switched into us.
static PyObject *
fiber_switch_back(PyObject *op, PyObject *const *args, Py_ssize_t nargs)
{
    FiberObject *self = (FiberObject *)op;
    if (nargs > 1) {
        PyErr_SetString(PyExc_TypeError, "switch_back() takes at most 1 argument");
        return NULL;
    }
    PyObject *value = nargs ? args[0] : Py_None;
    if (self != current) {
        PyErr_SetString(PyExc_ValueError, "fiber is not running");
        return NULL;
    }
    assert(self->transfer == NULL);
    self->transfer = Py_NewRef(value);
    if (jump(self->parent) < 0) {
        Py_CLEAR(self->transfer);
        return NULL;
    }
    // Resumed by fiber_switch_impl() with a value, or thrown into.
    PyObject *result = self->transfer;
    self->transfer = NULL;
    return result;   // NULL with an exception set if thrown into
}

static PyObject *
fiber_get_frame(PyObject *op, void *Py_UNUSED(closure))
{
    FiberObject *self = (FiberObject *)op;
    PyFrameObject *frame = PyUnstable_ExecStack_GetFrame(self->stack);
    if (frame == NULL) {
        Py_RETURN_NONE;
    }
    return (PyObject *)frame;
}

static PyObject *
fiber_get_state(PyObject *op, void *Py_UNUSED(closure))
{
    FiberObject *self = (FiberObject *)op;
    return PyLong_FromLong(PyUnstable_ExecStack_GetState(self->stack));
}

// How many bytes of C stack are spilled to the heap right now: non-zero
// exactly while the fiber is suspended (started and not running).
static PyObject *
fiber_get_saved_stack_size(PyObject *op, void *Py_UNUSED(closure))
{
    FiberObject *self = (FiberObject *)op;
    return PyLong_FromSize_t(self->saved_size);
}

static PyObject *
fiber_get_context(PyObject *op, void *Py_UNUSED(closure))
{
    FiberObject *self = (FiberObject *)op;
    PyObject *ctx = PyUnstable_ExecStack_GetContext(self->stack);
    if (ctx == NULL) {
        Py_RETURN_NONE;
    }
    return ctx;
}

static int
fiber_set_context(PyObject *op, PyObject *value, void *Py_UNUSED(closure))
{
    FiberObject *self = (FiberObject *)op;
    if (value == Py_None) {
        value = NULL;
    }
    return PyUnstable_ExecStack_SetContext(self->stack, value);
}

static PyObject *
fiber_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *run;
    if (!PyArg_ParseTuple(args, "O", &run)) {
        return NULL;
    }
    FiberObject *self = (FiberObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        return NULL;
    }
    self->run = Py_NewRef(run);
    self->stack = PyUnstable_ExecStack_New(NULL);
    if (self->stack == NULL) {
        Py_DECREF(self);
        return NULL;
    }
    return (PyObject *)self;
}

static int
fiber_traverse(PyObject *op, visitproc visit, void *arg)
{
    FiberObject *self = (FiberObject *)op;
    Py_VISIT(self->run);
    Py_VISIT(self->transfer);
    if (self->stack != NULL) {
        int err = PyUnstable_ExecStack_Traverse(self->stack, visit, arg);
        if (err) {
            return err;
        }
    }
    return 0;
}

static int
fiber_clear(PyObject *op)
{
    FiberObject *self = (FiberObject *)op;
    Py_CLEAR(self->run);
    Py_CLEAR(self->transfer);
    if (self->stack != NULL
        && PyUnstable_ExecStack_GetState(self->stack) != PyUnstable_ExecStack_RUNNING)
    {
        if (PyUnstable_ExecStack_Clear(self->stack) < 0) {
            PyErr_FormatUnraisable("Exception ignored in fiber_clear");
        }
    }
    return 0;
}

static void
fiber_dealloc(PyObject *op)
{
    FiberObject *self = (FiberObject *)op;
    PyObject_GC_UnTrack(op);
    (void)fiber_clear(op);
    if (self->stack != NULL) {
        if (PyUnstable_ExecStack_Free(self->stack) < 0) {
            PyErr_FormatUnraisable("Exception ignored in fiber_dealloc");
        }
    }
    PyMem_RawFree(self->saved);
    PyTypeObject *tp = Py_TYPE(op);
    tp->tp_free(op);
    Py_DECREF(tp);
}

static PyMethodDef fiber_methods[] = {
    {"switch", _PyCFunction_CAST(fiber_switch), METH_FASTCALL, NULL},
    {"throw", fiber_throw, METH_O, NULL},
    {"switch_back", _PyCFunction_CAST(fiber_switch_back), METH_FASTCALL, NULL},
    {NULL},
};

static PyGetSetDef fiber_getset[] = {
    {"frame", fiber_get_frame, NULL, NULL, NULL},
    {"state", fiber_get_state, NULL, NULL, NULL},
    {"saved_stack_size", fiber_get_saved_stack_size, NULL, NULL, NULL},
    {"context", fiber_get_context, fiber_set_context, NULL, NULL},
    {NULL},
};

static PyType_Slot fiber_slots[] = {
    {Py_tp_new, fiber_new},
    {Py_tp_dealloc, fiber_dealloc},
    {Py_tp_traverse, fiber_traverse},
    {Py_tp_clear, fiber_clear},
    {Py_tp_methods, fiber_methods},
    {Py_tp_getset, fiber_getset},
    {0, NULL},
};

static PyType_Spec fiber_spec = {
    .name = "_testcapi.Fiber",
    .basicsize = sizeof(FiberObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .slots = fiber_slots,
};

#endif  // HAVE_FIBER_TEST


static PyObject *
execstack_root_state(PyObject *self, PyObject *Py_UNUSED(args))
{
    PyThreadState *tstate = PyThreadState_Get();
    PyUnstable_ExecStack *root = PyUnstable_ThreadState_GetExecStack(tstate);
    if (root == NULL) {
        return NULL;
    }
    int state = PyUnstable_ExecStack_GetState(root);
    PyUnstable_ExecStack_Free(root);
    return PyLong_FromLong(state);
}

static PyObject *
execstack_root_frame(PyObject *self, PyObject *Py_UNUSED(args))
{
    PyThreadState *tstate = PyThreadState_Get();
    PyUnstable_ExecStack *root = PyUnstable_ThreadState_GetExecStack(tstate);
    if (root == NULL) {
        return NULL;
    }
    PyFrameObject *frame = PyUnstable_ExecStack_GetFrame(root);
    PyUnstable_ExecStack_Free(root);
    if (frame == NULL) {
        Py_RETURN_NONE;
    }
    return (PyObject *)frame;
}

static PyObject *
execstack_root_misuse(PyObject *self, PyObject *Py_UNUSED(args))
{
    // Deallocating or clearing the running root stack must fail cleanly.
    PyThreadState *tstate = PyThreadState_Get();
    PyUnstable_ExecStack *root = PyUnstable_ThreadState_GetExecStack(tstate);
    if (root == NULL) {
        return NULL;
    }
    // Releasing the reference we just took is fine...
    if (PyUnstable_ExecStack_Free(root) < 0) {
        return NULL;
    }
    // ...but releasing the thread state's own reference is not.
    if (PyUnstable_ExecStack_Free(root) == 0) {
        PyErr_SetString(PyExc_AssertionError, "Free(root) succeeded");
        return NULL;
    }
    if (!PyErr_ExceptionMatches(PyExc_SystemError)) {
        return NULL;
    }
    PyErr_Clear();
    if (PyUnstable_ExecStack_Clear(root) == 0) {
        PyErr_SetString(PyExc_AssertionError, "Clear(root) succeeded");
        return NULL;
    }
    if (!PyErr_ExceptionMatches(PyExc_SystemError)) {
        return NULL;
    }
    PyErr_Clear();
    if (PyUnstable_ExecStack_Switch(tstate, root) != root) {
        PyErr_SetString(PyExc_AssertionError, "Switch(root) is not a no-op");
        return NULL;
    }
    if (PyUnstable_ExecStack_Resume(tstate, root) == 0) {
        PyErr_SetString(PyExc_AssertionError, "Resume without Suspend succeeded");
        return NULL;
    }
    if (!PyErr_ExceptionMatches(PyExc_SystemError)) {
        return NULL;
    }
    PyErr_Clear();
    // Suspend/Resume round trip on the same stack.
    if (PyUnstable_ExecStack_Suspend(tstate) != root) {
        return NULL;
    }
    if (PyUnstable_ThreadState_GetExecStack(tstate) != NULL) {
        PyErr_SetString(PyExc_AssertionError, "GetExecStack succeeded while suspended");
        return NULL;
    }
    if (!PyErr_ExceptionMatches(PyExc_SystemError)) {
        return NULL;
    }
    PyErr_Clear();
    if (PyUnstable_ExecStack_Resume(tstate, root) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static void
execstack_capsule_destructor(PyObject *capsule)
{
    PyUnstable_ExecStack *stack = PyCapsule_GetPointer(capsule, "execstack");
    if (stack != NULL && PyUnstable_ExecStack_Free(stack) < 0) {
        PyErr_FormatUnraisable("Exception ignored freeing exec stack");
    }
}

// Take a reference to the current thread's running stack and wrap it in a
// capsule, so a test can keep the thread's root stack alive after the
// thread exits.
static PyObject *
execstack_hold_current(PyObject *self, PyObject *Py_UNUSED(args))
{
    PyThreadState *tstate = PyThreadState_Get();
    PyUnstable_ExecStack *stack = PyUnstable_ThreadState_GetExecStack(tstate);
    if (stack == NULL) {
        return NULL;
    }
    PyObject *capsule = PyCapsule_New(stack, "execstack",
                                      execstack_capsule_destructor);
    if (capsule == NULL) {
        PyUnstable_ExecStack_Free(stack);
    }
    return capsule;
}

static PyObject *
execstack_held_state(PyObject *self, PyObject *capsule)
{
    PyUnstable_ExecStack *stack = PyCapsule_GetPointer(capsule, "execstack");
    if (stack == NULL) {
        return NULL;
    }
    PyFrameObject *frame = PyUnstable_ExecStack_GetFrame(stack);
    PyObject *result = Py_BuildValue("(iN)",
                                     PyUnstable_ExecStack_GetState(stack),
                                     frame ? (PyObject *)frame : Py_NewRef(Py_None));
    return result;
}

static PyObject *
execstack_new_bad_context(PyObject *self, PyObject *arg)
{
    PyUnstable_ExecStack *stack = PyUnstable_ExecStack_New(arg);
    if (stack == NULL) {
        return NULL;
    }
    PyUnstable_ExecStack_Free(stack);
    Py_RETURN_NONE;
}

static PyMethodDef test_methods[] = {
    {"execstack_root_state", execstack_root_state, METH_NOARGS, NULL},
    {"execstack_root_frame", execstack_root_frame, METH_NOARGS, NULL},
    {"execstack_root_misuse", execstack_root_misuse, METH_NOARGS, NULL},
    {"execstack_new_bad_context", execstack_new_bad_context, METH_O, NULL},
    {"execstack_hold_current", execstack_hold_current, METH_NOARGS, NULL},
    {"execstack_held_state", execstack_held_state, METH_O, NULL},
    {NULL},
};

int
_PyTestCapi_Init_ExecStack(PyObject *m)
{
    if (PyModule_AddFunctions(m, test_methods) < 0) {
        return -1;
    }
    if (PyModule_AddIntConstant(m, "EXECSTACK_NEW", PyUnstable_ExecStack_NEW) < 0
        || PyModule_AddIntConstant(m, "EXECSTACK_RUNNING", PyUnstable_ExecStack_RUNNING) < 0
        || PyModule_AddIntConstant(m, "EXECSTACK_SUSPENDED", PyUnstable_ExecStack_SUSPENDED) < 0)
    {
        return -1;
    }
#ifdef HAVE_FIBER_TEST
    PyObject *type = PyType_FromSpec(&fiber_spec);
    if (type == NULL) {
        return -1;
    }
    if (PyModule_AddObjectRef(m, "Fiber", type) < 0) {
        Py_DECREF(type);
        return -1;
    }
    Py_DECREF(type);
#endif
    return 0;
}
