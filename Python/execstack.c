/* Execution stacks: the per-coroutine part of a PyThreadState.
 *
 * See the comment block in Include/cpython/pystate.h for the API contract.
 *
 * A PyThreadState has exactly one RUNNING _PyExecStack. While a stack is
 * RUNNING its saved fields are meaningless: the live values are in the thread
 * state. PyUnstable_ExecStack_Switch() moves the per-stack fields of the
 * thread state into the running stack (making it SUSPENDED) and then loads
 * the target.
 *
 * The frame chain of a suspended stack needs special care. The eval loop keeps
 * an "entry frame" (_PyEntryFrame in ceval.c) as a C local every time it is
 * entered, and links it into the chain via ->previous. Stack switching
 * libraries copy their C stacks to the heap while suspended, so following a
 * ->previous link to an entry frame of a suspended stack would read garbage.
 * On suspend we therefore unlink entry frames from the chain (recording what
 * we changed) while the C stack is still intact; on resume we put them back.
 * The visible chain of a suspended stack thus consists only of frames living
 * on the data stack or in generator objects, which are safe to walk.
 */

#include "Python.h"
#include "pycore_critical_section.h"  // _PyCriticalSection_SuspendAll()
#include "pycore_frame.h"             // PyFrameObject internals
#include "pycore_interpframe.h"       // _PyFrame_GetFirstComplete()
#include "pycore_obmalloc.h"          // _PyObject_VirtualFree()
#include "pycore_pystate.h"           // _PyThreadState_GET()
#include "pycore_stackref.h"          // _PyCStackRef
#include "pycore_tstate.h"            // _PyThreadStateImpl


// One rewritten ->previous link.
typedef struct {
    _PyInterpreterFrame *frame;     // frame whose ->previous we rewrote
    _PyInterpreterFrame *previous;  // the original value (an entry frame)
} execstack_link;

struct _PyExecStack {
    PyThreadState *tstate;   // thread this stack runs/ran on, NULL if NEW
    Py_ssize_t refs;         // owners: New()/GetExecStack() callers, tstate
    int state;               // PyUnstable_ExecStack_State
    int is_root;             // created for a thread's original stack
    int cleared;             // Clear() was called; cannot be switched to

    /* Saved per-stack fields, valid only while SUSPENDED (context is also
       valid while NEW). */
    PyObject *context;                     // strong
    _PyInterpreterFrame *current_frame;    // raw tstate->current_frame
    _PyInterpreterFrame *top_frame;        // first non-entry frame of the chain
    _PyErr_StackItem *exc_info;            // NULL means &tstate->exc_state
    _PyErr_StackItem exc_state;            // exc_value is a strong ref
    int py_recursion_depth;
    int tracing;
    int what_event;
    PyObject *delete_later;                // borrowed (refcount 0 objects)
    _PyStackChunk *datastack_chunk;
    PyObject **datastack_top;
    PyObject **datastack_limit;
    PyObject *current_executor;            // borrowed
    PyObject *asyncio_running_task;        // strong
    uintptr_t critical_section;
#ifdef Py_GIL_DISABLED
    _PyCStackRef *c_stack_refs;
    // Strong references to the objects held by the C-stack refs above,
    // taken at suspension so the GC can see them while the C stack is gone.
    PyObject **c_stack_ref_snapshot;
    Py_ssize_t c_stack_ref_count;
#endif

    /* Entry frame links rewritten at suspension. */
    execstack_link *links;
    Py_ssize_t n_links;
    Py_ssize_t links_capacity;
};


static inline int
is_cstack_frame(_PyInterpreterFrame *frame)
{
    return frame->owner >= FRAME_OWNED_BY_INTERPRETER;
}

static PyUnstable_ExecStack *
execstack_alloc(void)
{
    PyUnstable_ExecStack *stack = PyMem_RawCalloc(1, sizeof(*stack));
    if (stack == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    stack->state = PyUnstable_ExecStack_NEW;
    stack->refs = 1;
    return stack;
}

static void
execstack_dealloc(PyUnstable_ExecStack *stack)
{
    PyMem_RawFree(stack->links);
#ifdef Py_GIL_DISABLED
    PyMem_RawFree(stack->c_stack_ref_snapshot);
#endif
    PyMem_RawFree(stack);
}

// Return the stack running on tstate (borrowed), creating the root stack
// if needed. The thread state holds the root's initial reference.
static PyUnstable_ExecStack *
get_running(PyThreadState *tstate)
{
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;
    if (impl->exec_stack != NULL) {
        return impl->exec_stack;
    }
    if (impl->root_exec_stack != NULL) {
        PyErr_SetString(PyExc_SystemError,
                        "no execution stack is running: between "
                        "PyUnstable_ExecStack_Suspend() and _Resume()");
        return NULL;
    }
    PyUnstable_ExecStack *root = execstack_alloc();
    if (root == NULL) {
        return NULL;
    }
    root->tstate = tstate;
    root->is_root = 1;
    root->state = PyUnstable_ExecStack_RUNNING;
    impl->exec_stack = root;
    impl->root_exec_stack = root;
    return root;
}


/* Entry frame unlinking ---------------------------------------------------- */

// Count how many ->previous links in the chain starting at `frame` point at
// entry frames, so we can reserve space before mutating anything.
static Py_ssize_t
count_entry_links(_PyInterpreterFrame *frame)
{
    Py_ssize_t n = 0;
    // Skip leading entry frames (current_frame can transiently be one).
    while (frame != NULL && is_cstack_frame(frame)) {
        frame = frame->previous;
    }
    while (frame != NULL) {
        _PyInterpreterFrame *prev = frame->previous;
        if (prev != NULL && is_cstack_frame(prev)) {
            n++;
            while (prev != NULL && is_cstack_frame(prev)) {
                prev = prev->previous;
            }
        }
        frame = prev;
    }
    return n;
}

static int
reserve_links(PyUnstable_ExecStack *stack, Py_ssize_t n)
{
    if (n <= stack->links_capacity) {
        return 0;
    }
    execstack_link *links = PyMem_RawRealloc(stack->links,
                                             n * sizeof(execstack_link));
    if (links == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    stack->links = links;
    stack->links_capacity = n;
    return 0;
}

// Must be called while the C stack holding the entry frames is intact.
// reserve_links() must have been called with the result of
// count_entry_links() for the same chain.
static void
unlink_entry_frames(PyUnstable_ExecStack *stack)
{
    _PyInterpreterFrame *frame = stack->current_frame;
    while (frame != NULL && is_cstack_frame(frame)) {
        frame = frame->previous;
    }
    stack->top_frame = frame;
    stack->n_links = 0;
    while (frame != NULL) {
        _PyInterpreterFrame *prev = frame->previous;
        if (prev != NULL && is_cstack_frame(prev)) {
            assert(stack->n_links < stack->links_capacity);
            execstack_link *link = &stack->links[stack->n_links++];
            link->frame = frame;
            link->previous = prev;
            while (prev != NULL && is_cstack_frame(prev)) {
                prev = prev->previous;
            }
            frame->previous = prev;
        }
        frame = prev;
    }
}

static void
relink_entry_frames(PyUnstable_ExecStack *stack)
{
    for (Py_ssize_t i = 0; i < stack->n_links; i++) {
        stack->links[i].frame->previous = stack->links[i].previous;
    }
    stack->n_links = 0;
    stack->top_frame = NULL;
}


/* Free-threaded C stack refs ----------------------------------------------- */

#ifdef Py_GIL_DISABLED
static int
snapshot_c_stack_refs(PyUnstable_ExecStack *stack, _PyCStackRef *head)
{
    Py_ssize_t n = 0;
    for (_PyCStackRef *node = head; node != NULL; node = node->next) {
        if (!PyStackRef_IsNullOrInt(node->ref)) {
            n++;
        }
    }
    assert(stack->c_stack_ref_snapshot == NULL);
    if (n == 0) {
        return 0;
    }
    PyObject **objs = PyMem_RawMalloc(n * sizeof(PyObject *));
    if (objs == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    Py_ssize_t i = 0;
    for (_PyCStackRef *node = head; node != NULL; node = node->next) {
        if (!PyStackRef_IsNullOrInt(node->ref)) {
            objs[i++] = Py_NewRef(PyStackRef_AsPyObjectBorrow(node->ref));
        }
    }
    stack->c_stack_ref_snapshot = objs;
    stack->c_stack_ref_count = n;
    return 0;
}

static void
drop_c_stack_ref_snapshot(PyUnstable_ExecStack *stack)
{
    PyObject **objs = stack->c_stack_ref_snapshot;
    Py_ssize_t n = stack->c_stack_ref_count;
    stack->c_stack_ref_snapshot = NULL;
    stack->c_stack_ref_count = 0;
    for (Py_ssize_t i = 0; i < n; i++) {
        // These objects use deferred reference counting (that is why they
        // were on the C stack ref list), so Py_DECREF will not run finalizers.
        Py_DECREF(objs[i]);
    }
    PyMem_RawFree(objs);
}
#endif


/* Save / load -------------------------------------------------------------- */

// Move the per-stack fields out of tstate into `stack`. Leaves tstate
// inconsistent; must be followed by load_state() or init_fresh_state().
static void
save_state(PyUnstable_ExecStack *stack, PyThreadState *tstate)
{
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;

    stack->context = tstate->context;
    tstate->context = NULL;

    stack->current_frame = tstate->current_frame;
    unlink_entry_frames(stack);

    stack->exc_info =
        tstate->exc_info == &tstate->exc_state ? NULL : tstate->exc_info;
    stack->exc_state = tstate->exc_state;

    stack->py_recursion_depth =
        tstate->py_recursion_limit - tstate->py_recursion_remaining;
    stack->tracing = tstate->tracing;
    stack->what_event = tstate->what_event;
    stack->delete_later = tstate->delete_later;
    stack->datastack_chunk = tstate->datastack_chunk;
    stack->datastack_top = tstate->datastack_top;
    stack->datastack_limit = tstate->datastack_limit;
    stack->current_executor = tstate->current_executor;

    stack->asyncio_running_task = impl->asyncio_running_task;
    impl->asyncio_running_task = NULL;

    // Release the locks held by critical sections on the stack we are
    // leaving, exactly as detaching from the thread would. Otherwise the
    // stack we switch to could deadlock trying to take one of them.
    if (tstate->critical_section != 0) {
        _PyCriticalSection_SuspendAll(tstate);
    }
    stack->critical_section = tstate->critical_section;

#ifdef Py_GIL_DISABLED
    stack->c_stack_refs = impl->c_stack_refs;
#endif
}

// Install the saved fields of `stack` into tstate and forget them.
static void
load_state(PyThreadState *tstate, PyUnstable_ExecStack *stack)
{
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;

    tstate->context = stack->context;
    stack->context = NULL;
    // Invalidate the contextvars cache, which would otherwise stay valid
    // across the switch.
    tstate->context_ver++;

    relink_entry_frames(stack);
    tstate->current_frame = stack->current_frame;
    stack->current_frame = NULL;

    tstate->exc_state = stack->exc_state;
    tstate->exc_info = stack->exc_info ? stack->exc_info : &tstate->exc_state;
    stack->exc_info = NULL;
    memset(&stack->exc_state, 0, sizeof(stack->exc_state));

    tstate->py_recursion_remaining =
        tstate->py_recursion_limit - stack->py_recursion_depth;
    tstate->tracing = stack->tracing;
    tstate->what_event = stack->what_event;
    tstate->delete_later = stack->delete_later;
    stack->delete_later = NULL;
    tstate->datastack_chunk = stack->datastack_chunk;
    tstate->datastack_top = stack->datastack_top;
    tstate->datastack_limit = stack->datastack_limit;
    stack->datastack_chunk = NULL;
    stack->datastack_top = NULL;
    stack->datastack_limit = NULL;
    tstate->current_executor = stack->current_executor;
    stack->current_executor = NULL;

    assert(impl->asyncio_running_task == NULL);
    impl->asyncio_running_task = stack->asyncio_running_task;
    stack->asyncio_running_task = NULL;

    tstate->critical_section = stack->critical_section;
    stack->critical_section = 0;
    if (tstate->critical_section != 0) {
        _PyCriticalSection_Resume(tstate);
    }

#ifdef Py_GIL_DISABLED
    impl->c_stack_refs = stack->c_stack_refs;
    stack->c_stack_refs = NULL;
    drop_c_stack_ref_snapshot(stack);
#endif
}

// Set up tstate for a stack that has never run. Must be called right after
// save_state(): recursion depth, tracing depth and the error indicator are
// inherited from the stack we are coming from and are left untouched.
static void
init_fresh_state(PyThreadState *tstate, PyUnstable_ExecStack *stack)
{
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;

    tstate->context = stack->context;
    stack->context = NULL;
    tstate->context_ver++;

    tstate->current_frame = NULL;
    memset(&tstate->exc_state, 0, sizeof(tstate->exc_state));
    tstate->exc_info = &tstate->exc_state;
    tstate->delete_later = NULL;
    // A NULL datastack makes the frame allocator start a fresh root chunk.
    tstate->datastack_chunk = NULL;
    tstate->datastack_top = NULL;
    tstate->datastack_limit = NULL;
    tstate->current_executor = NULL;
    assert(impl->asyncio_running_task == NULL);
    tstate->critical_section = 0;
#ifdef Py_GIL_DISABLED
    // Do not inherit the parent's C stack refs: they live on the parent's C
    // stack, which this stack is about to overwrite.
    impl->c_stack_refs = NULL;
#endif
    (void)impl;
}


/* Clearing ----------------------------------------------------------------- */

static void
free_datastack(PyUnstable_ExecStack *stack)
{
    _PyStackChunk *chunk = stack->datastack_chunk;
    stack->datastack_chunk = NULL;
    stack->datastack_top = NULL;
    stack->datastack_limit = NULL;
    while (chunk != NULL) {
        _PyStackChunk *prev = chunk->previous;
        _PyObject_VirtualFree(chunk, chunk->size);
        chunk = prev;
    }
}

// Clear the frames of a suspended stack, top-down. Generator frames are
// skipped: their generator owns them. Clearing may run finalizers, so detach
// the chain from the stack object before starting.
static void
clear_frames(PyUnstable_ExecStack *stack)
{
    _PyInterpreterFrame *frame = stack->top_frame;
    stack->top_frame = NULL;
    stack->current_frame = NULL;
    stack->n_links = 0;
    while (frame != NULL) {
        _PyInterpreterFrame *prev = frame->previous;
        assert(!is_cstack_frame(frame));
        if (frame->owner == FRAME_OWNED_BY_THREAD) {
            _PyFrame_ClearExceptCode(frame);
            PyStackRef_CLEAR(frame->f_executable);
        }
        frame = prev;
    }
}

// Release everything a non-running stack owns.
static void
clear_state(PyUnstable_ExecStack *stack)
{
    assert(stack->state != PyUnstable_ExecStack_RUNNING);
    stack->cleared = 1;
    Py_CLEAR(stack->context);
    if (stack->state == PyUnstable_ExecStack_SUSPENDED) {
        clear_frames(stack);
        Py_CLEAR(stack->exc_state.exc_value);
        stack->exc_state.previous_item = NULL;
        stack->exc_info = NULL;
        Py_CLEAR(stack->asyncio_running_task);
        // delete_later: a chain of refcount-zero objects awaiting the
        // trashcan. There is no way to finish destroying them without the
        // thread state they belong to; they leak. In practice this is
        // unreachable: a switch would have to happen inside tp_dealloc.
        stack->delete_later = NULL;
        stack->current_executor = NULL;
        stack->critical_section = 0;
        free_datastack(stack);
#ifdef Py_GIL_DISABLED
        stack->c_stack_refs = NULL;
        drop_c_stack_ref_snapshot(stack);
#endif
    }
}


/* Public API --------------------------------------------------------------- */

PyUnstable_ExecStack *
PyUnstable_ExecStack_New(PyObject *context)
{
    if (context != NULL && !PyContext_CheckExact(context)) {
        PyErr_Format(PyExc_TypeError,
                     "expected a Context object or NULL, got %T", context);
        return NULL;
    }
    PyUnstable_ExecStack *stack = execstack_alloc();
    if (stack == NULL) {
        return NULL;
    }
    stack->context = Py_XNewRef(context);
    return stack;
}

PyUnstable_ExecStack *
PyUnstable_ThreadState_GetExecStack(PyThreadState *tstate)
{
    assert(tstate != NULL);
    PyUnstable_ExecStack *stack = get_running(tstate);
    if (stack != NULL) {
        stack->refs++;
    }
    return stack;
}

PyUnstable_ExecStack_State
PyUnstable_ExecStack_GetState(PyUnstable_ExecStack *stack)
{
    return (PyUnstable_ExecStack_State)stack->state;
}

static int
check_resume_target(PyThreadState *tstate, PyUnstable_ExecStack *target)
{
    if (target->state == PyUnstable_ExecStack_RUNNING) {
        PyErr_SetString(PyExc_SystemError,
                        "execution stack is already running");
        return -1;
    }
    if (target->cleared) {
        PyErr_SetString(PyExc_SystemError,
                        "execution stack has been cleared");
        return -1;
    }
    if (target->tstate != NULL && target->tstate != tstate) {
        PyErr_SetString(PyExc_SystemError,
                        "execution stack belongs to a different thread");
        return -1;
    }
    return 0;
}

PyUnstable_ExecStack *
PyUnstable_ExecStack_Suspend(PyThreadState *tstate)
{
    assert(tstate != NULL);
    assert(tstate == _PyThreadState_GET());
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;

    PyUnstable_ExecStack *current = get_running(tstate);
    if (current == NULL) {
        return NULL;
    }

    // Reserve what we need before mutating anything so that failure leaves
    // the thread state untouched.
    Py_ssize_t n_links = count_entry_links(tstate->current_frame);
    if (reserve_links(current, n_links) < 0) {
        return NULL;
    }
#ifdef Py_GIL_DISABLED
    if (snapshot_c_stack_refs(current, impl->c_stack_refs) < 0) {
        return NULL;
    }
#endif

    save_state(current, tstate);
    current->state = PyUnstable_ExecStack_SUSPENDED;
    // No stack is running until Resume(). get_running() recognises this
    // state by exec_stack == NULL with root_exec_stack != NULL.
    impl->exec_stack = NULL;
    return current;
}

int
PyUnstable_ExecStack_Resume(PyThreadState *tstate, PyUnstable_ExecStack *target)
{
    assert(tstate != NULL);
    assert(tstate == _PyThreadState_GET());
    assert(target != NULL);
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;

    if (impl->exec_stack != NULL || impl->root_exec_stack == NULL) {
        PyErr_SetString(PyExc_SystemError,
                        "PyUnstable_ExecStack_Resume() called without a "
                        "preceding PyUnstable_ExecStack_Suspend()");
        return -1;
    }
    if (check_resume_target(tstate, target) < 0) {
        return -1;
    }

    if (target->state == PyUnstable_ExecStack_NEW) {
        target->tstate = tstate;
        init_fresh_state(tstate, target);
    }
    else {
        assert(target->state == PyUnstable_ExecStack_SUSPENDED);
        load_state(tstate, target);
    }
    target->state = PyUnstable_ExecStack_RUNNING;
    impl->exec_stack = target;
    return 0;
}

PyUnstable_ExecStack *
PyUnstable_ExecStack_Switch(PyThreadState *tstate, PyUnstable_ExecStack *target)
{
    assert(tstate != NULL);
    assert(tstate == _PyThreadState_GET());
    assert(target != NULL);

    PyUnstable_ExecStack *current = get_running(tstate);
    if (current == NULL) {
        return NULL;
    }
    if (target == current) {
        return current;
    }
    // Validate the target first so that a failure leaves everything as is.
    if (check_resume_target(tstate, target) < 0) {
        return NULL;
    }
    if (PyUnstable_ExecStack_Suspend(tstate) == NULL) {
        return NULL;
    }
    int err = PyUnstable_ExecStack_Resume(tstate, target);
    (void)err;
    assert(err == 0);  // target was validated and Resume cannot fail otherwise
    return current;
}

PyFrameObject *
PyUnstable_ExecStack_GetFrame(PyUnstable_ExecStack *stack)
{
    _PyInterpreterFrame *frame;
    switch (stack->state) {
    case PyUnstable_ExecStack_RUNNING:
        return PyThreadState_GetFrame(stack->tstate);
    case PyUnstable_ExecStack_SUSPENDED:
        frame = _PyFrame_GetFirstComplete(stack->top_frame);
        break;
    default:
        return NULL;
    }
    if (frame == NULL) {
        return NULL;
    }
    PyFrameObject *f = _PyFrame_GetFrameObject(frame);
    if (f == NULL) {
        PyErr_Clear();
        return NULL;
    }
    return (PyFrameObject *)Py_NewRef(f);
}

PyObject *
PyUnstable_ExecStack_GetContext(PyUnstable_ExecStack *stack)
{
    if (stack->state == PyUnstable_ExecStack_RUNNING) {
        return Py_XNewRef(stack->tstate->context);
    }
    return Py_XNewRef(stack->context);
}

int
PyUnstable_ExecStack_SetContext(PyUnstable_ExecStack *stack, PyObject *context)
{
    if (context != NULL && !PyContext_CheckExact(context)) {
        PyErr_Format(PyExc_TypeError,
                     "expected a Context object or NULL, got %T", context);
        return -1;
    }
    if (stack->state == PyUnstable_ExecStack_RUNNING) {
        PyThreadState *tstate = stack->tstate;
        if (tstate != _PyThreadState_GET()) {
            PyErr_SetString(PyExc_SystemError,
                            "cannot set the context of a stack running on "
                            "another thread");
            return -1;
        }
        Py_XSETREF(tstate->context, Py_XNewRef(context));
        tstate->context_ver++;
        return 0;
    }
    Py_XSETREF(stack->context, Py_XNewRef(context));
    return 0;
}

int
PyUnstable_ExecStack_Traverse(PyUnstable_ExecStack *stack, visitproc visit,
                              void *arg)
{
    Py_VISIT(stack->context);
    if (stack->state != PyUnstable_ExecStack_SUSPENDED) {
        return 0;
    }
    Py_VISIT(stack->exc_state.exc_value);
    Py_VISIT(stack->asyncio_running_task);
    for (_PyInterpreterFrame *f = stack->top_frame; f != NULL; f = f->previous) {
        assert(!is_cstack_frame(f));
        // Generator frames are traversed by their generator.
        if (f->owner != FRAME_OWNED_BY_THREAD) {
            continue;
        }
        if (f->stackpointer == NULL) {
            // GH-129236: the eval loop may have handed the stack pointer to
            // a C local. Visit what we safely can.
            Py_VISIT(f->frame_obj);
            Py_VISIT(f->f_locals);
            _Py_VISIT_STACKREF(f->f_funcobj);
            _Py_VISIT_STACKREF(f->f_executable);
            continue;
        }
        int err = _PyFrame_Traverse(f, visit, arg);
        if (err) {
            return err;
        }
    }
#ifdef Py_GIL_DISABLED
    for (Py_ssize_t i = 0; i < stack->c_stack_ref_count; i++) {
        Py_VISIT(stack->c_stack_ref_snapshot[i]);
    }
#endif
    return 0;
}

int
PyUnstable_ExecStack_Clear(PyUnstable_ExecStack *stack)
{
    if (stack->state == PyUnstable_ExecStack_RUNNING) {
        PyErr_SetString(PyExc_SystemError,
                        "cannot clear a running execution stack");
        return -1;
    }
    clear_state(stack);
    return 0;
}

int
PyUnstable_ExecStack_Free(PyUnstable_ExecStack *stack)
{
    assert(stack->refs > 0);
    if (stack->refs == 1 && stack->state == PyUnstable_ExecStack_RUNNING) {
        // Only the thread state itself may drop the last reference to a
        // running stack (see _PyExecStack_ClearThreadState).
        PyErr_SetString(PyExc_SystemError,
                        "cannot free a running execution stack");
        return -1;
    }
    if (--stack->refs > 0) {
        return 0;
    }
    clear_state(stack);
    execstack_dealloc(stack);
    return 0;
}


/* Thread state teardown ---------------------------------------------------- */

void
_PyExecStack_ClearThreadState(PyThreadState *tstate)
{
    _PyThreadStateImpl *impl = (_PyThreadStateImpl *)tstate;
    PyUnstable_ExecStack *running = impl->exec_stack;
    PyUnstable_ExecStack *root = impl->root_exec_stack;
    if (root == NULL) {
        assert(running == NULL);
        return;
    }
    impl->exec_stack = NULL;
    impl->root_exec_stack = NULL;

    if (running != NULL) {
        // The running stack's live state is in tstate and will be released
        // by PyThreadState_Clear. Leave the object empty and detached so
        // that any remaining owner can still inspect and free it.
        running->state = PyUnstable_ExecStack_SUSPENDED;
        running->tstate = NULL;
        running->cleared = 1;
        running->current_frame = NULL;
        running->top_frame = NULL;
        running->n_links = 0;
        memset(&running->exc_state, 0, sizeof(running->exc_state));
    }
    // (running == NULL: the thread died between Suspend() and Resume();
    // nothing can be done about any stack other than the root.)

    if (running != root) {
        // The thread is exiting while a user stack is running. The root
        // stack is suspended and owns the thread's original frames.
        clear_state(root);
        root->tstate = NULL;
    }
    // Drop the thread state's reference to the root.
    assert(root->refs > 0);
    if (--root->refs == 0) {
        execstack_dealloc(root);
    }
}
