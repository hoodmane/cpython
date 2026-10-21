/* Tests for the PyUnstable_ExecStack API.
 *
 * To exercise the API realistically we implement a minimal stackful
 * coroutine ("Fiber") on top of POSIX ucontext: each fiber runs Python code
 * on its own C stack and its own execution stack. This mirrors what greenlet
 * and Pyodide do, including the entry frames that the eval loop keeps on the
 * C stack between Python frames.
 */

#include "parts.h"
#include "util.h"

#include "frameobject.h"

#if defined(__linux__)
#  define HAVE_FIBER_TEST 1
#  include <ucontext.h>
#  include <sys/mman.h>
#endif

#ifdef HAVE_FIBER_TEST

#define FIBER_STACK_SIZE (1024 * 1024)

typedef struct FiberObject {
    PyObject_HEAD
    PyUnstable_ExecStack *stack;
    PyUnstable_ExecStack *parent_stack;   // stack that last switched into us
    struct FiberObject *parent_fiber;     // NULL if that was the root
    ucontext_t ctx;
    ucontext_t parent_ctx;
    void *c_stack;
    PyObject *run;
    PyObject *transfer;   // value passed across a switch; NULL + error = throw
    int started;
    int finished;
} FiberObject;

// The fiber currently executing on this (test) thread, NULL for the root.
// The tests are single threaded apart from one that exits a thread with a
// suspended fiber, which never runs concurrently with the others.
static FiberObject *current_fiber = NULL;
static FiberObject *starting_fiber = NULL;

static void
apply_stack_protection(PyThreadState *tstate, FiberObject *fiber)
{
    if (fiber == NULL) {
        PyUnstable_ThreadState_ResetStackProtection(tstate);
    }
    else {
        if (PyUnstable_ThreadState_SetStackProtection(tstate, fiber->c_stack,
                                                      FIBER_STACK_SIZE) < 0) {
            Py_FatalError("SetStackProtection failed");
        }
    }
}

// Runs on the fiber's own C stack.
static void
fiber_entry(void)
{
    FiberObject *self = starting_fiber;
    starting_fiber = NULL;
    PyThreadState *tstate = PyThreadState_Get();

    self->parent_stack = PyUnstable_ExecStack_Switch(tstate, self->stack);
    if (self->parent_stack == NULL) {
        Py_FatalError("PyUnstable_ExecStack_Switch failed in fiber_entry");
    }
    apply_stack_protection(tstate, self);

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

    if (PyUnstable_ExecStack_Switch(tstate, self->parent_stack) == NULL) {
        Py_FatalError("PyUnstable_ExecStack_Switch failed leaving fiber");
    }
    apply_stack_protection(tstate, self->parent_fiber);
    // Returning jumps to ctx.uc_link == &self->parent_ctx.
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
    if (self == current_fiber) {
        Py_XDECREF(value);
        PyErr_Clear();
        PyErr_SetString(PyExc_ValueError, "fiber is already running");
        return NULL;
    }
    assert(self->transfer == NULL);
    self->transfer = value;
    self->parent_fiber = current_fiber;
    current_fiber = self;
    if (!self->started) {
        self->started = 1;
        getcontext(&self->ctx);
        self->ctx.uc_stack.ss_sp = self->c_stack;
        self->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
        self->ctx.uc_link = &self->parent_ctx;
        makecontext(&self->ctx, fiber_entry, 0);
        starting_fiber = self;
    }
    swapcontext(&self->parent_ctx, &self->ctx);
    // Back from the fiber. It already switched the exec stack back to ours.
    current_fiber = self->parent_fiber;
    PyObject *result = self->transfer;
    self->transfer = NULL;
    assert((result == NULL) == (PyErr_Occurred() != NULL));
    return result;
}

static PyObject *
fiber_switch(PyObject *op, PyObject *args)
{
    PyObject *value = Py_None;
    if (!PyArg_ParseTuple(args, "|O", &value)) {
        return NULL;
    }
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
fiber_switch_back(PyObject *op, PyObject *args)
{
    FiberObject *self = (FiberObject *)op;
    PyObject *value = Py_None;
    if (!PyArg_ParseTuple(args, "|O", &value)) {
        return NULL;
    }
    if (self != current_fiber) {
        PyErr_SetString(PyExc_ValueError, "fiber is not running");
        return NULL;
    }
    PyThreadState *tstate = PyThreadState_Get();
    assert(self->transfer == NULL);
    self->transfer = Py_NewRef(value);

    if (PyUnstable_ExecStack_Switch(tstate, self->parent_stack) == NULL) {
        Py_FatalError("PyUnstable_ExecStack_Switch failed in switch_back");
    }
    apply_stack_protection(tstate, self->parent_fiber);
    swapcontext(&self->ctx, &self->parent_ctx);

    // Resumed by fiber_switch_impl().
    self->parent_stack = PyUnstable_ExecStack_Switch(tstate, self->stack);
    if (self->parent_stack == NULL) {
        Py_FatalError("PyUnstable_ExecStack_Switch failed resuming fiber");
    }
    apply_stack_protection(tstate, self);
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
    self->c_stack = mmap(NULL, FIBER_STACK_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (self->c_stack == MAP_FAILED) {
        self->c_stack = NULL;
        Py_DECREF(self);
        return PyErr_NoMemory();
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
    if (self->c_stack != NULL) {
        munmap(self->c_stack, FIBER_STACK_SIZE);
    }
    PyTypeObject *tp = Py_TYPE(op);
    tp->tp_free(op);
    Py_DECREF(tp);
}

static PyMethodDef fiber_methods[] = {
    {"switch", fiber_switch, METH_VARARGS, NULL},
    {"throw", fiber_throw, METH_O, NULL},
    {"switch_back", fiber_switch_back, METH_VARARGS, NULL},
    {NULL},
};

static PyGetSetDef fiber_getset[] = {
    {"frame", fiber_get_frame, NULL, NULL, NULL},
    {"state", fiber_get_state, NULL, NULL, NULL},
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
    return PyLong_FromLong(PyUnstable_ExecStack_GetState(root));
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
    if (frame == NULL) {
        Py_RETURN_NONE;
    }
    return (PyObject *)frame;
}

static PyObject *
execstack_root_misuse(PyObject *self, PyObject *Py_UNUSED(args))
{
    // Freeing or clearing the running root stack must fail cleanly.
    PyThreadState *tstate = PyThreadState_Get();
    PyUnstable_ExecStack *root = PyUnstable_ThreadState_GetExecStack(tstate);
    if (root == NULL) {
        return NULL;
    }
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
    Py_RETURN_NONE;
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
