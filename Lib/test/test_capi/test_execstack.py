"""Tests for the PyUnstable_ExecStack API.

The C side (Modules/_testcapi/execstack.c) implements a minimal greenlet-like
stackful coroutine, ``Fiber``. All fibers of a thread share one C stack
region; a suspended fiber's slice of it is copied to the heap and the region
is poisoned, so the entry frames the eval loop left on a suspended fiber's C
stack are garbage until it resumes. Anything that followed them would crash.
"""

import contextvars
import gc
import sys
import textwrap
import threading
import unittest
import weakref

from test.support import import_helper, threading_helper, requires_jit_enabled

_testcapi = import_helper.import_module('_testcapi')
_testinternalcapi = import_helper.import_module('_testinternalcapi')

NEW = _testcapi.EXECSTACK_NEW
RUNNING = _testcapi.EXECSTACK_RUNNING
SUSPENDED = _testcapi.EXECSTACK_SUSPENDED


class RootStackTests(unittest.TestCase):
    def test_root_is_running(self):
        self.assertEqual(_testcapi.execstack_root_state(), RUNNING)

    def test_root_frame_is_current_frame(self):
        frame = _testcapi.execstack_root_frame()
        self.assertIs(frame, sys._getframe())

    def test_root_misuse(self):
        _testcapi.execstack_root_misuse()

    def test_hold_root_in_same_thread(self):
        cap = _testcapi.execstack_hold_current()
        state, frame = _testcapi.execstack_held_state(cap)
        self.assertEqual(state, RUNNING)
        self.assertIs(frame, sys._getframe())
        del cap   # drops our extra reference; the thread keeps its own
        self.assertEqual(_testcapi.execstack_root_state(), RUNNING)

    @threading_helper.requires_working_threading()
    def test_root_outlives_thread(self):
        holder = []

        def body():
            holder.append(_testcapi.execstack_hold_current())

        t = threading.Thread(target=body)
        t.start()
        t.join()
        cap = holder.pop()
        # The thread is gone: its root stack is detached and empty.
        state, frame = _testcapi.execstack_held_state(cap)
        self.assertEqual(state, SUSPENDED)
        self.assertIsNone(frame)
        del cap   # last reference: deallocates

    def test_new_rejects_non_context(self):
        with self.assertRaises(TypeError):
            _testcapi.execstack_new_bad_context(42)
        # A real context is fine.
        _testcapi.execstack_new_bad_context(contextvars.copy_context())


@unittest.skipUnless(hasattr(_testcapi, 'Fiber'), 'needs ucontext fibers')
class FiberTests(unittest.TestCase):
    Fiber = getattr(_testcapi, 'Fiber', None)

    def frame_names(self, frame):
        return [code.co_name for code in self.frame_codes(frame)]

    def frame_codes(self, frame):
        codes = []
        while frame is not None:
            codes.append(frame.f_code)
            frame = frame.f_back
        return codes

    def test_basic_switching(self):
        log = []

        def run(fib, arg):
            log.append(('start', arg))
            x = fib.switch_back('a')
            log.append(('resumed', x))
            y = fib.switch_back('b')
            log.append(('resumed', y))
            return 'ret'

        f = self.Fiber(run)
        self.assertEqual(f.state, NEW)
        self.assertIsNone(f.frame)
        self.assertEqual(f.switch(1), 'a')
        self.assertEqual(f.state, SUSPENDED)
        self.assertEqual(f.switch(2), 'b')
        self.assertEqual(f.switch(3), 'ret')
        self.assertEqual(log, [('start', 1), ('resumed', 2), ('resumed', 3)])
        with self.assertRaises(ValueError):
            f.switch()

    def test_frames_are_isolated(self):
        """A fiber's frame chain does not include the frames that started it,
        and the parent's chain does not include the fiber's."""
        seen = {}

        def inner(fib):
            seen['inside'] = self.frame_names(sys._getframe())
            fib.switch_back()

        def run(fib, arg):
            inner(fib)

        def outer():
            f = self.Fiber(run)
            f.switch()
            # Suspended: GetFrame gives the fiber's top frame, and f_back
            # walks only the fiber's own frames.
            seen['suspended'] = self.frame_names(f.frame)
            seen['parent'] = self.frame_codes(sys._getframe())
            return f

        f = outer()
        self.assertEqual(seen['inside'], ['inner', 'run'])
        self.assertEqual(seen['suspended'], ['inner', 'run'])
        self.assertNotIn(run.__code__, seen['parent'])
        self.assertIn(outer.__code__, seen['parent'])
        # Finish it so nothing is left suspended at dealloc.
        f.switch()

    def test_nested_eval_loops(self):
        """Suspending from inside a C call that re-entered the interpreter
        leaves several entry frames on the C stack. The suspended chain must
        skip all of them and resuming must restore them."""
        def key(fib, x):
            if x == 2:
                fib.switch_back('in key')
            return x

        def run(fib, arg):
            # sorted() -> key() is a C -> Python re-entry.
            # list(map(...)) adds another level around it.
            return list(map(lambda x: sorted([3, 2, 1], key=lambda v: key(fib, v))[0], [0]))

        f = self.Fiber(run)
        self.assertEqual(f.switch(), 'in key')
        names = self.frame_names(f.frame)
        self.assertEqual(names, ['key', '<lambda>', '<lambda>', 'run'])
        self.assertEqual(f.switch(), [1])

    def test_switch_from_finalizer_during_frame_pop(self):
        """tstate->current_frame is transiently an entry frame while the eval
        loop clears a returning frame; a finalizer running then may switch."""
        events = []

        class Finalizer:
            def __init__(self, fib):
                self.fib = fib
            def __del__(self):
                events.append('del')
                self.fib.switch_back('from __del__')
                events.append('del resumed')

        def leaf(fib):
            local = Finalizer(fib)
            return 1

        def run(fib, arg):
            # leaf's frame is popped inside the eval loop; `local` dies then.
            # Use a C call boundary so that an entry frame sits above leaf.
            return list(map(lambda _: leaf(fib), [0]))

        f = self.Fiber(run)
        self.assertEqual(f.switch(), 'from __del__')
        self.assertEqual(events, ['del'])
        # The visible chain must be walkable.
        self.frame_names(f.frame)
        self.assertEqual(f.switch(), [1])
        self.assertEqual(events, ['del', 'del resumed'])

    def test_exception_state_is_per_stack(self):
        def run(fib, arg):
            try:
                raise KeyError('inner')
            except KeyError:
                self.assertIsInstance(sys.exception(), KeyError)
                fib.switch_back()
                # Our handled exception is back after resuming.
                self.assertIsInstance(sys.exception(), KeyError)
            return 'ok'

        f = self.Fiber(run)
        try:
            raise ValueError('outer')
        except ValueError:
            f.switch()
            # The fiber's KeyError must not leak into our except block.
            self.assertIsInstance(sys.exception(), ValueError)
        self.assertIsNone(sys.exception())
        self.assertEqual(f.switch(), 'ok')

    def test_throw(self):
        def run(fib, arg):
            try:
                fib.switch_back()
            except ZeroDivisionError as e:
                return ('caught', str(e))

        f = self.Fiber(run)
        f.switch()
        self.assertEqual(f.throw(ZeroDivisionError('x')), ('caught', 'x'))

    def test_throw_propagates(self):
        def run(fib, arg):
            fib.switch_back()

        f = self.Fiber(run)
        f.switch()
        with self.assertRaises(ZeroDivisionError):
            f.throw(ZeroDivisionError())
        self.assertEqual(f.state, SUSPENDED)
        with self.assertRaises(ValueError):
            f.switch()  # finished

    def test_throw_into_new(self):
        def run(fib, arg):
            self.fail('should not run')

        f = self.Fiber(run)
        with self.assertRaises(ZeroDivisionError):
            f.throw(ZeroDivisionError())

    def test_exception_propagates_out(self):
        def run(fib, arg):
            raise OSError('from fiber')

        f = self.Fiber(run)
        with self.assertRaisesRegex(OSError, 'from fiber'):
            f.switch()

    def test_contextvars(self):
        var = contextvars.ContextVar('var', default='default')

        def run(fib, arg):
            self.assertEqual(var.get(), 'default')
            var.set('fiber')
            fib.switch_back()
            self.assertEqual(var.get(), 'fiber')
            return 'ok'

        def main():
            f = self.Fiber(run)
            self.assertIsNone(f.context)
            var.set('main')
            f.switch()
            self.assertEqual(var.get(), 'main')
            self.assertIsInstance(f.context, contextvars.Context)
            self.assertEqual(f.context[var], 'fiber')
            self.assertEqual(f.switch(), 'ok')

        # Run in a copied context so the test leaves no trace in the
        # thread's own context (keeps refleak hunting quiet).
        contextvars.copy_context().run(main)

    def test_set_context(self):
        var = contextvars.ContextVar('var', default='default')

        def run(fib, arg):
            return var.get()

        ctx = contextvars.copy_context()
        ctx.run(var.set, 'preset')
        f = self.Fiber(run)
        f.context = ctx
        self.assertIs(f.context, ctx)
        self.assertEqual(f.switch(), 'preset')
        with self.assertRaises(TypeError):
            self.Fiber(run).context = 42

    def test_recursion_depth_is_per_stack(self):
        depth = 100

        def recurse(fib, n):
            if n == 0:
                fib.switch_back()
                return 0
            return recurse(fib, n - 1) + 1

        def run(fib, arg):
            return recurse(fib, depth)

        f = self.Fiber(run)
        before = sys.getrecursionlimit()
        try:
            sys.setrecursionlimit(depth * 2 + 50)
            f.switch()
            # Deep inside the fiber. We must still be able to recurse here
            # almost up to the limit ourselves.
            def shallow(n):
                return 0 if n == 0 else shallow(n - 1) + 1
            self.assertEqual(shallow(depth), depth)
            self.assertEqual(f.switch(), depth)
        finally:
            sys.setrecursionlimit(before)

    def test_gc_collects_cycle_through_suspended_frame(self):
        """A suspended fiber whose frame refers back to the fiber is a cycle
        that only the GC can break, and only if it can see into the frame."""
        class Marker:
            pass

        refs = []

        def run(fib, arg):
            marker = Marker()     # local on the suspended frame only
            refs.append(weakref.ref(marker))
            me = fib              # cycle: frame -> fiber -> stack -> frame
            fib.switch_back()
            return marker, me

        f = self.Fiber(run)
        f.switch()
        wr = refs[0]
        gc.collect()
        self.assertIsNotNone(wr())   # kept alive by the suspended frame
        del f
        gc.collect()
        self.assertIsNone(wr())

    def test_gc_sees_objects_only_referenced_from_suspended_frames(self):
        """gc.get_referrers() must find the suspended frame's owner."""
        class Marker:
            pass

        def run(fib, arg):
            marker = arg
            fib.switch_back()
            return marker

        marker = Marker()
        f = self.Fiber(run)
        f.switch(marker)
        referrers = gc.get_referrers(marker)
        self.assertTrue(any(r is f or (isinstance(r, type(sys._getframe()))
                                       and r.f_code is run.__code__)
                            for r in referrers), referrers)
        self.assertIs(f.switch(), marker)

    def test_dealloc_while_suspended_clears_frames(self):
        events = []

        class Local:
            def __del__(self):
                events.append('local deleted')

        def run(fib, arg):
            local = Local()
            fib.switch_back()
            events.append('resumed')

        f = self.Fiber(run)
        f.switch()
        frame = f.frame
        self.assertEqual(frame.f_code, run.__code__)
        del f
        gc.collect()
        self.assertEqual(events, ['local deleted'])
        # The frame object we kept is still valid (it took ownership of its
        # data) but is finished.
        self.assertEqual(frame.f_code, run.__code__)
        self.assertIsNone(frame.f_back)

    def test_frame_object_kept_across_suspend_resume(self):
        def run(fib, arg):
            fr = sys._getframe()
            fib.switch_back(fr)
            return fr

        f = self.Fiber(run)
        fr = f.switch()
        self.assertIs(f.frame, fr)
        self.assertIs(f.switch(), fr)
        self.assertIsNone(fr.f_back)

    def test_many_fibers(self):
        def run(fib, n):
            total = 0
            while n:
                total += n
                n = fib.switch_back(total)
            return total

        fibers = [self.Fiber(run) for _ in range(50)]
        results = [f.switch(i + 1) for i, f in enumerate(fibers)]
        self.assertEqual(results, [i + 1 for i in range(50)])
        results = [f.switch(10) for f in fibers]
        self.assertEqual(results, [i + 11 for i in range(50)])
        results = [f.switch(0) for f in fibers]
        self.assertEqual(results, [i + 11 for i in range(50)])

    def test_nested_fibers(self):
        def inner(fib, arg):
            fib.switch_back('inner suspended')
            return 'inner done'

        def outer(fib, arg):
            g = self.Fiber(inner)
            r1 = g.switch()
            fib.switch_back(('outer suspended', r1))
            r2 = g.switch()
            return ('outer done', r2)

        f = self.Fiber(outer)
        self.assertEqual(f.switch(), ('outer suspended', 'inner suspended'))
        self.assertEqual(f.switch(), ('outer done', 'inner done'))

    def test_generator_suspended_inside_fiber(self):
        def gen(fib):
            yield 1
            fib.switch_back('in gen')
            yield 2

        def run(fib, arg):
            g = gen(fib)
            return list(g)

        f = self.Fiber(run)
        self.assertEqual(f.switch(), 'in gen')
        names = self.frame_names(f.frame)
        self.assertEqual(names, ['gen', 'run'])
        self.assertEqual(f.switch(), [1, 2])

    def test_sys_getframe_in_fiber(self):
        def run(fib, arg):
            fib.switch_back()
            # sys._getframe(1) must not see the parent's frames.
            with self.assertRaises(ValueError):
                sys._getframe(1)
            return 'ok'

        f = self.Fiber(run)
        f.switch()
        self.assertEqual(f.switch(), 'ok')

    def test_c_stack_is_spilled_while_suspended(self):
        def run(fib, arg):
            fib.switch_back()

        f = self.Fiber(run)
        self.assertEqual(f.saved_stack_size, 0)
        f.switch()
        # Suspended inside the eval loop: a few C frames at least.
        suspended = f.saved_stack_size
        self.assertGreater(suspended, 1000)
        f.switch()
        # Finished: only the tail of fiber_entry() is left spilled.
        self.assertLess(f.saved_stack_size, suspended)

    def test_frames_survive_many_switches(self):
        """Every switch overwrites the region; walking suspended frames after
        each one checks that nothing points at the C stack."""
        def run(fib, n):
            def deeper(k):
                if k == 0:
                    return fib.switch_back(n)
                return deeper(k - 1)
            return deeper(n)

        fibers = [self.Fiber(run) for _ in range(8)]
        for i, f in enumerate(fibers):
            self.assertEqual(f.switch(i), i)
        for _ in range(3):
            for i, f in enumerate(fibers):
                names = self.frame_names(f.frame)
                self.assertEqual(names, ['deeper'] * (i + 1) + ['run'])
                gc.collect()
        for f in fibers:
            f.switch()

    @threading_helper.requires_working_threading()
    def test_switch_from_another_thread_rejected(self):
        def run(fib, arg):
            fib.switch_back()

        f = self.Fiber(run)
        f.switch()
        errors = []

        def body():
            try:
                f.switch()
            except ValueError as e:
                errors.append(str(e))

        t = threading.Thread(target=body)
        t.start()
        t.join()
        self.assertEqual(errors, ['cannot switch to a fiber of another thread'])
        f.switch()

    @threading_helper.requires_working_threading()
    def test_thread_exits_with_suspended_fiber(self):
        holder = []

        def run(fib, arg):
            fib.switch_back()
            self.fail('never resumed')

        def thread_body():
            f = self.Fiber(run)
            f.switch()
            holder.append(f)

        t = threading.Thread(target=thread_body)
        t.start()
        t.join()
        f = holder.pop()
        self.assertEqual(f.state, SUSPENDED)
        # The thread state, and the base_frame sentinel embedded in it, are
        # gone. Walking the suspended chain must not touch them.
        self.assertEqual(self.frame_names(f.frame), ['run'])
        gc.collect()
        with self.assertRaises(ValueError):
            # Not running in this thread; our fiber impl refuses. The stack
            # itself belongs to a dead thread and must be freeable below.
            f.switch_back()
        del f
        gc.collect()


@unittest.skipUnless(hasattr(_testcapi, 'Fiber'), 'needs fibers')
@requires_jit_enabled
class FiberJITTests(unittest.TestCase):
    """The JIT keeps per-thread state that is really per execution stack:
    the executor currently running (current_executor / jit_exit) and the
    trace being recorded (jit_tracer_state).

    sys._jit.is_active() reads tstate->current_executor, so a leak across a
    switch is observable. Like test_sys.test_jit_is_active, each scenario
    runs in a fresh subprocess: executor creation depends on warmup counters
    and invalidations that other tests in the same process (and refleak
    repetitions) disturb. Within the scripts, loop bodies are kept small
    enough to be traced and nothing writes to module globals, since the
    globals watcher would invalidate every executor of the module.
    """

    PRELUDE = textwrap.dedent("""
        import gc, sys, _testcapi, _testinternalcapi
        THRESHOLD = _testinternalcapi.TIER2_THRESHOLD
        # The loop is traced at iteration THRESHOLD and runs as JIT code from
        # THRESHOLD + 1 on (see test_sys.test_jit_is_active).
        ITERATIONS = THRESHOLD + 200

        def run_fiber(fib, arg):
            log = []
            for i in range(ITERATIONS):
                log.append(sys._jit.is_active())
                fib.switch_back(None)
                log.append(sys._jit.is_active())
            return log

        # Same thing with a code object of its own, so two fibers can run
        # JIT code without sharing an executor (see check_log).
        def run_fiber2(fib, arg):
            log = []
            for i in range(ITERATIONS):
                log.append(sys._jit.is_active())
                fib.switch_back(None)
                log.append(sys._jit.is_active())
            return log

        def drive(fibers):
            # Switch into each fiber in turn until all have returned their
            # log, recording our own view before and after each switch.
            logs = {}
            root_log = []
            while len(logs) < len(fibers):
                for f in fibers:
                    if f in logs:
                        continue
                    root_log.append(sys._jit.is_active())
                    r = f.switch()
                    root_log.append(sys._jit.is_active())
                    if r is not None:
                        logs[f] = r
            return logs, root_log

        def check_log(name, log, expect_active, strict=True):
            pairs = list(zip(log[::2], log[1::2]))
            # Being in JIT code after a switch without having been in JIT
            # code before it means the other side's executor leaked across.
            leaked = [(b, a) for b, a in pairs if a and not b]
            assert not leaked, (name, leaked[:5])
            if strict:
                # The converse can legitimately happen if our executor was
                # invalidated while we were suspended (we then deopt on
                # resume). With distinct code objects and nothing else
                # creating executors that does not occur, so require the
                # view to be exactly unchanged.
                bad = [(b, a) for b, a in pairs if b != a]
                assert not bad, (name, bad[:5])
            active = sum(b for b, a in pairs)
            if expect_active:
                assert active > 100, (name, active)
    """)

    def run_script(self, body):
        from test.support.script_helper import assert_python_ok
        assert_python_ok("-c", self.PRELUDE + textwrap.dedent(body))

    def test_current_executor_is_per_stack(self):
        self.run_script("""
            f = _testcapi.Fiber(run_fiber)
            logs, root_log = drive([f])
            check_log('fiber', logs[f], expect_active=True)
            check_log('root', root_log, expect_active=False)
        """)

    def test_switching_between_fibers_in_jit_code(self):
        self.run_script("""
            a, b = _testcapi.Fiber(run_fiber), _testcapi.Fiber(run_fiber2)
            logs, root_log = drive([a, b])
            check_log('a', logs[a], expect_active=True)
            check_log('b', logs[b], expect_active=True)
            check_log('root', root_log, expect_active=False)
        """)

    def test_concurrent_traces_of_the_same_loop(self):
        # Two fibers running the same code object both get hot at the same
        # time. With per-stack tracers each records its own trace of the
        # loop; the second executor to be installed replaces the first, so
        # one fiber deopts once on resume. That is fine, but no executor may
        # leak across a switch and both fibers must end up in JIT code.
        self.run_script("""
            a, b = _testcapi.Fiber(run_fiber), _testcapi.Fiber(run_fiber)
            logs, root_log = drive([a, b])
            check_log('a', logs[a], expect_active=True, strict=False)
            check_log('b', logs[b], expect_active=True, strict=False)
            check_log('root', root_log, expect_active=False)
        """)

    def test_trace_in_progress_travels_with_stack(self):
        # Suspending a fiber while the tracer is recording must not stop the
        # rest of the thread from tracing, even if that fiber is never
        # resumed and is garbage collected with the half-recorded trace.
        self.run_script("""
            import types

            def run(fib, arg):
                for i in range(THRESHOLD * 3):
                    fib.switch_back(i)

            # Leave fibers suspended at every iteration around the point
            # where the trace is recorded, so one is suspended mid-trace.
            # Each gets a copy of the code object: the warmup counter lives
            # in the code object, so with a shared one only the first fiber
            # to get hot would record a trace.
            leftovers = []
            for n in range(THRESHOLD - 2, THRESHOLD + 4):
                fresh = types.FunctionType(run.__code__.replace(),
                                           run.__globals__)
                f = _testcapi.Fiber(fresh)
                for _ in range(n):
                    f.switch()
                leftovers.append(f)
            del leftovers
            gc.collect()

            # A fresh loop on this thread must still get JIT compiled.
            def hot():
                for i in range(ITERATIONS):
                    if sys._jit.is_active():
                        return i
                return None
            assert hot() is not None
        """)


if __name__ == '__main__':
    unittest.main()
