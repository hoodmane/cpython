// Tests for the asynchronous parts of the JavaScript FFI: awaiting promises
// from Python, awaiting Python awaitables from JavaScript, async iteration in
// both directions, async context managers and the WebLoop. These need the
// JavaScript event loop to turn, so they are driven from JavaScript rather
// than from Lib/test/test_pyodide.py.
//
// Run with: node --test Platforms/emscripten/node_test/

import { test } from "node:test";
import assert from "node:assert/strict";
import { pyEval, pyRun, runPython, sleep } from "./harness.mjs";

function isPythonError(e, type) {
  return e?.constructor?.name === "PythonError" && e.type === type;
}

test("await a Python coroutine from JavaScript", async () => {
  const { f } = pyRun(
    `
async def f(x):
    await asyncio.sleep(0.01)
    return x + 1
`,
    "f",
  );
  assert.equal(await f(6), 7);
  f.destroy();
});

test("then/catch/finally on a PyProxy of a coroutine", async () => {
  const { ok, bad } = pyRun(
    `
async def ok():
    return 2

async def bad():
    raise ValueError("hi")
`,
    "ok",
    "bad",
  );
  let finallyCalled = false;
  assert.equal(
    await ok().then((x) => x * 10).finally(() => (finallyCalled = true)),
    20,
  );
  assert.ok(finallyCalled);
  const err = await bad().catch((e) => e);
  assert.ok(isPythonError(err, "ValueError"), err);
  await assert.rejects(bad(), (e) => isPythonError(e, "ValueError"));
  ok.destroy();
  bad.destroy();
});

test("PyProxy of an awaitable is destroyed when scheduled", async () => {
  const { f } = pyRun(
    `
async def f():
    return 1
`,
    "f",
  );
  const coro = f();
  const promise = coro.then((x) => x);
  // _ensure_future destroys the proxy, but then/catch/finally keep working.
  assert.throws(() => coro.type, /destroyed/);
  assert.equal(await promise, 1);
  assert.equal(await coro, 1);
  f.destroy();
});

test("calling a coroutine function from JavaScript schedules it", async () => {
  const { f, ran } = pyRun(
    `
ran = []
async def f():
    ran.append(1)
`,
    "f",
    "ran",
  );
  f();
  assert.equal(ran.length, 0);
  await sleep(1);
  assert.equal(ran.length, 1);
  f.destroy();
  ran.destroy();
});

test("Python awaits a JavaScript promise", async () => {
  globalThis.resolveLater = (x) =>
    new Promise((resolve) => setTimeout(() => resolve(x), 5));
  const { f } = pyRun(
    `
async def f():
    a = await js.Promise.resolve(3)
    b = await js.resolveLater(4)
    c = await js.Promise.resolve(js.Promise.resolve(5))
    return to_js([a, b, c])
`,
    "f",
  );
  assert.deepEqual(await f(), [3, 4, 5]);
  f.destroy();
  delete globalThis.resolveLater;
});

test("Python awaits a rejected JavaScript promise", async () => {
  globalThis.rejectLater = () =>
    new Promise((_, reject) =>
      setTimeout(() => reject(new Error("This is an error message!")), 5),
    );
  const { f } = pyRun(
    `
async def f():
    try:
        await js.rejectLater()
    except JsException as e:
        return str(e)
`,
    "f",
  );
  assert.match(await f(), /This is an error message!/);
  f.destroy();
  delete globalThis.rejectLater;
});

test("Python error propagates through a JavaScript promise", async () => {
  // A Python exception raised in a callback that rejects a JavaScript promise
  // arrives in Python as a JsException wrapping the PythonError.
  const { f } = pyRun(
    `
async def inner(_):
    raise KeyError("key")

async def f():
    try:
        await js.Promise.resolve().then(create_once_callable(inner))
    except JsException as e:
        return str(e)
`,
    "f",
  );
  assert.match(await f(), /KeyError: 'key'/);
  f.destroy();
});

test("arguments of an async JavaScript call are destroyed when it settles", async () => {
  globalThis.asyncStash = async (x) => {
    globalThis.stashed = x;
    await sleep(5);
    return x.toString();
  };
  const { f } = pyRun(
    `
async def f():
    d = {1: 2}
    res = await js.asyncStash(d)
    # The argument proxy was alive while the function was running
    assert res == str(d), res
    # ... but it's destroyed once the promise resolves.
    try:
        js.stashed.toString()
    except JsException as e:
        return str(e)
    return "not destroyed"
`,
    "f",
  );
  assert.match(
    await f(),
    /automatically destroyed at the end of an asynchronous function call/,
  );
  f.destroy();
  delete globalThis.asyncStash;
  delete globalThis.stashed;
});

test("then/catch/finally_ on a JsPromise", async () => {
  // Indexing rather than calling so that we get a JsPromise rather than a
  // Future.
  const rejected = Promise.reject(new Error("no"));
  // Avoid an unhandled rejection in node before Python attaches handlers.
  rejected.catch(() => {});
  globalThis.promises = [Promise.resolve(10), rejected];
  const { f } = pyRun(
    `
results = []
def onfulfilled(v):
    results.append(("fulfilled", v))
def onrejected(e):
    results.append(("rejected", e.message))
def onfinally():
    results.append(("finally",))

async def f():
    p1, p2 = js.promises[0], js.promises[1]
    await p1.then(onfulfilled, onrejected).finally_(onfinally)
    await p2.catch(onrejected)
    await p2.then(onfulfilled).catch(onrejected)
    # No handler for the fulfilled case: the value passes through.
    results.append(await p1.catch(onrejected))
    return to_js(results)
`,
    "f",
  );
  assert.deepEqual(await f(), [
    ["fulfilled", 10],
    ["finally"],
    ["rejected", "no"],
    ["rejected", "no"],
    10,
  ]);
  f.destroy();
  delete globalThis.promises;
});

test("PyodideFuture then/catch/finally_", async () => {
  const { f } = pyRun(
    `
async def f():
    fut = loop.create_future()
    assert isinstance(fut, PyodideFuture)
    r = fut.then(lambda x: x + 1).then(lambda x: x * 2)
    fut.set_result(3)
    a = await r

    fut2 = loop.create_future()
    r2 = fut2.catch(lambda e: repr(e))
    fut2.set_exception(ValueError("oops"))
    b = await r2

    calls = []
    fut3 = loop.create_future()
    r3 = fut3.finally_(lambda: calls.append(1))
    fut3.set_result(7)
    c = await r3
    return to_js([a, b, c, calls])
`,
    "f",
  );
  assert.deepEqual(await f(), [8, "ValueError('oops')", 7, [1]]);
  f.destroy();
});

test("async for over a JavaScript async generator", async () => {
  globalThis.agen = async function* () {
    yield 1;
    await sleep(1);
    yield 2;
    yield 3;
  };
  const { f } = pyRun(
    `
async def f():
    from pyodide.ffi import JsAsyncGenerator, JsAsyncIterator, JsIterator
    g = js.agen()
    assert isinstance(g, JsAsyncGenerator)
    assert isinstance(g, JsAsyncIterator)
    assert not isinstance(g, JsIterator)
    result = []
    async for x in g:
        result.append(x)
    return to_js(result)
`,
    "f",
  );
  assert.deepEqual(await f(), [1, 2, 3]);
  f.destroy();
  delete globalThis.agen;
});

test("aiter over a JavaScript async iterable", async () => {
  globalThis.aiterable = {
    [Symbol.asyncIterator]() {
      return (async function* () {
        yield 1;
        yield 2;
      })();
    },
  };
  const { f } = pyRun(
    `
async def f():
    from pyodide.ffi import JsAsyncIterable, JsIterable, JsAsyncIterator
    it = js.aiterable
    assert isinstance(it, JsAsyncIterable)
    assert not isinstance(it, JsIterable)
    try:
        iter(it)
    except TypeError:
        pass
    else:
        raise AssertionError("expected TypeError")
    ait = aiter(it)
    assert isinstance(ait, JsAsyncIterator)
    result = [await anext(ait), await anext(ait)]
    try:
        await anext(ait)
    except StopAsyncIteration:
        result.append("done")
    return to_js(result)
`,
    "f",
  );
  assert.deepEqual(await f(), [1, 2, "done"]);
  f.destroy();
  delete globalThis.aiterable;
});

test("asend, athrow and aclose on a JavaScript async generator", async () => {
  globalThis.makeAgen = async function* () {
    let n = 0;
    for (let i = 0; i < 3; i++) {
      n = yield n + 2;
    }
  };
  globalThis.makeAgenFinally = async function* (log) {
    try {
      yield 1;
      yield 2;
      log.push("this never happens");
    } finally {
      log.push("finally");
    }
  };
  const { f } = pyRun(
    `
async def f():
    it = js.makeAgen()
    result = [await it.asend(None), await it.asend(2), await it.asend(3)]
    try:
        await it.asend(4)
    except StopAsyncIteration:
        result.append("done")

    log = []
    g = js.makeAgenFinally(create_proxy(log))
    result.append(await anext(g))
    result.append(await g.aclose())
    result.append(await g.aclose())
    result.append(log)

    g = js.makeAgenFinally(create_proxy([]))
    await anext(g)
    try:
        await g.athrow(TypeError("hi"))
    except JsException as e:
        result.append("hi" in str(e))
    return to_js(result)
`,
    "f",
  );
  assert.deepEqual(await f(), [
    2,
    4,
    5,
    "done",
    1,
    undefined,
    undefined,
    ["finally"],
    true,
  ]);
  f.destroy();
  delete globalThis.makeAgen;
  delete globalThis.makeAgenFinally;
});

test("argument lifetimes of a JavaScript async generator", async () => {
  globalThis.agenArgs = async function* (x) {
    let l = [x];
    l.push(yield);
    l.push(yield);
    return l.map((x) => x.toString());
  };
  const { f } = pyRun(
    `
async def f():
    g = js.agenArgs({1})
    await g.asend(None)
    await g.asend({2})
    try:
        await g.asend({3})
    except StopAsyncIteration as e:
        return to_js(e.args[0])
`,
    "f",
  );
  assert.deepEqual(await f(), ["{1}", "{2}", "{3}"]);
  f.destroy();
  delete globalThis.agenArgs;
});

test("for await over a Python async generator", async () => {
  const { gen } = pyRun(
    `
async def gen():
    yield 1
    await asyncio.sleep(0.001)
    yield 2
    yield 3
`,
    "gen",
  );
  const g = gen();
  assert.ok(Symbol.asyncIterator in g);
  const result = [];
  for await (const x of g) {
    result.push(x);
  }
  assert.deepEqual(result, [1, 2, 3]);
  g.destroy();
  gen.destroy();
});

test("for await over a Python async iterable", async () => {
  const { A } = pyRun(
    `
class A:
    def __aiter__(self):
        return self._gen()
    async def _gen(self):
        yield "a"
        yield "b"
`,
    "A",
  );
  const a = A();
  const result = [];
  for await (const x of a) {
    result.push(x);
  }
  assert.deepEqual(result, ["a", "b"]);
  a.destroy();
  A.destroy();
});

test("next/throw/return on a Python async generator", async () => {
  const { gen } = pyRun(
    `
async def gen():
    log = []
    try:
        x = yield 1
        log.append(x)
        yield 2
    except JsException as e:
        log.append(repr(e))
        yield 3
    finally:
        log.append("finally")
    yield to_js(log)
`,
    "gen",
  );
  let g = gen();
  assert.deepEqual(await g.next(), { done: false, value: 1 });
  assert.deepEqual(await g.next("sent"), { done: false, value: 2 });
  assert.deepEqual(await g.throw(new TypeError("hi")), {
    done: false,
    value: 3,
  });
  const res = await g.next();
  assert.equal(res.done, false);
  const log = res.value;
  assert.equal(log[0], "sent");
  assert.match(log[1], /TypeError/);
  assert.equal(log[2], "finally");
  assert.deepEqual(await g.next(), { done: true, value: undefined });
  g.destroy();

  g = gen();
  await g.next();
  assert.deepEqual(await g.return(7), { done: true, value: 7 });
  g.destroy();
  gen.destroy();
});

test("async with on a JavaScript object with Symbol.asyncDispose", async () => {
  globalThis.makeDisposable = (async) => ({
    x: 9,
    closed: false,
    [Symbol.asyncDispose]: async
      ? async function () {
          await sleep(5);
          this.closed = true;
        }
      : function () {
          this.closed = true;
        },
  });
  const { f } = pyRun(
    `
async def f(use_async):
    o = js.makeDisposable(use_async)
    async with o as o2:
        assert o2 is o
        assert o.x == 9
        assert not o.closed
    assert o.closed
    o = js.makeDisposable(use_async)
    try:
        async with o:
            raise Exception("oops")
    except Exception:
        pass
    assert o.closed
    return True
`,
    "f",
  );
  assert.equal(await f(true), true);
  assert.equal(await f(false), true);
  f.destroy();
  delete globalThis.makeDisposable;
});

test("WebLoop call_later and asyncio.sleep", async () => {
  const { f } = pyRun(
    `
async def f():
    import time
    t0 = time.monotonic()
    await asyncio.sleep(0.02)
    elapsed = time.monotonic() - t0
    assert elapsed >= 0.015, elapsed
    result = []
    loop.call_later(0.001, result.append, 1)
    h = loop.call_later(0.001, result.append, 2)
    h.cancel()
    loop.call_soon(result.append, 3)
    await asyncio.sleep(0.01)
    return to_js(sorted(result))
`,
    "f",
  );
  assert.deepEqual(await f(), [1, 3]);
  f.destroy();
});

test("asyncio.gather and tasks run on the WebLoop", async () => {
  const { f } = pyRun(
    `
async def g(x):
    await asyncio.sleep(0.001 * x)
    return x

async def f():
    t = asyncio.create_task(g(1))
    assert isinstance(t, PyodideTask)
    return to_js(await asyncio.gather(t, g(2), g(3)))
`,
    "f",
  );
  assert.deepEqual(await f(), [1, 2, 3]);
  f.destroy();
});

test("cancelling a task awaiting a JavaScript promise", async () => {
  globalThis.neverSettles = () => new Promise(() => {});
  const { f } = pyRun(
    `
async def waits():
    await js.neverSettles()

async def f():
    fut = asyncio.ensure_future(waits())
    await asyncio.sleep(0.001)
    fut.cancel()
    await asyncio.sleep(0.001)
    return fut.cancelled()
`,
    "f",
  );
  assert.equal(await f(), true);
  f.destroy();
  delete globalThis.neverSettles;
});

test("unhandled exception in a task calls the exception handler", async () => {
  const { f } = pyRun(
    `
async def bad():
    1 / 0

async def f():
    contexts = []
    loop.set_exception_handler(lambda loop, ctx: contexts.append(ctx))
    try:
        asyncio.create_task(bad())
        await asyncio.sleep(0.01)
    finally:
        loop.set_exception_handler(None)
    return to_js([type(ctx["exception"]).__name__ for ctx in contexts])
`,
    "f",
  );
  assert.deepEqual(await f(), ["ZeroDivisionError"]);
  f.destroy();
});

test("create_once_callable", async () => {
  const { f, calls } = pyRun(
    `
calls = []
def inner(x):
    calls.append(x)
    return x + 1
f = create_once_callable(inner)
`,
    "f",
    "calls",
  );
  assert.equal(f(1), 2);
  assert.throws(() => f(2), /OnceProxy can only be called once/);
  assert.equal(calls.length, 1);
  assert.equal(calls.get(0), 1);
  calls.destroy();
});

test("Python error in an async generator from JavaScript", async () => {
  const { gen } = pyRun(
    `
async def gen():
    yield 1
    raise ValueError("boom")
`,
    "gen",
  );
  const g = gen();
  assert.deepEqual(await g.next(), { done: false, value: 1 });
  await assert.rejects(g.next(), (e) => isPythonError(e, "ValueError"));
  g.destroy();
  gen.destroy();
});

test("finalize", () => {
  // Make sure nothing above left Python in a bad state.
  runPython("import gc; gc.collect()");
});
