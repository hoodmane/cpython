from unittest import TestCase
from _pyodide_core import run_js
from _pyodide import jsnull

JsError = type(run_js("new Error()"))


class ConversionTest(TestCase):
    def test_run_js(self):
        self.assertEqual(run_js("3 + 4"), 7)
        with self.assertRaisesRegex(JsError, "Error: Hi!"):
            run_js("throw new Error('Hi!')")

    def test_jsproxy_call(self):
        res = run_js("(x, y) => x + y")(1, 9)
        self.assertEqual(res, 10)

    def test_python2js(self):
        self.assertTrue(run_js("(x) => x === 7")(7))
        self.assertTrue(run_js("(x) => x === 2.3")(2.3))
        self.assertTrue(
            run_js("(x) => x === 77015781075109876017131518n")(
                77015781075109876017131518
            )
        )
        self.assertTrue(run_js("(x) => x === 'abc'")("abc"))
        self.assertTrue(run_js("(x) => x === undefined")(None))
        self.assertTrue(run_js("(x) => x === null")(jsnull))
        self.assertTrue(run_js("(x) => x === false")(False))
        self.assertTrue(run_js("(x) => x === true")(True))
        self.assertTrue(run_js("(x) => x === 'pyodidé'")("pyodidé"))
        self.assertTrue(run_js("(x) => x === '碘化物'")("碘化物"))
        self.assertTrue(run_js("(x) => x === '🐍'")("🐍"))

    def test_python2js_integers(self):
        x = 77015781075109876017131518
        js_str = run_js(f"(x) => x.toString()")
        js_typeof = run_js(f"(x) => typeof x")
        for _ in range(33):
            self.assertEqual(js_str(x), str(x))
            self.assertEqual(js_str(-x), str(-x))
            self.assertEqual(js_typeof(x), "bigint")
            self.assertEqual(js_typeof(-x), "bigint")
            x >>= 1
        for _ in range(32):
            self.assertEqual(js_str(x), str(x))
            self.assertEqual(js_str(-x), str(-x))
            self.assertEqual(js_typeof(x), "number")
            self.assertEqual(js_typeof(-x), "number")
            x >>= 1

    def test_js2python(self):
        self.assertEqual(run_js('"pyodidé"'), "pyodidé")
        self.assertEqual(run_js('"碘化物"'), "碘化物")
        self.assertEqual(run_js('"🐍"'), "🐍")
        self.assertEqual(run_js("2.3"), 2.3)
        self.assertEqual(
            run_js("77015781075109876017131518n"), 77015781075109876017131518
        )
        self.assertEqual(run_js("undefined"), None)
        self.assertEqual(run_js("null"), jsnull)
        self.assertEqual(run_js("false"), False)
        self.assertEqual(run_js("true"), True)

    def test_js2python_integers(self):
        x = 77015781075109876017131518
        while x != 0:
            self.assertEqual(run_js(f"{x}n"), x)
            self.assertEqual(run_js(f"-{x}n"), -x)
            x >>= 1

    def test_raise_through_js(self):
        def f():
            1 / 0

        with self.assertRaises(ZeroDivisionError):
            run_js("((f) => f())")(f)


class JsProxyTest(TestCase):
    def test_jsproxy(self):
        o = run_js("[7, 11, -1]")
        self.assertEqual(repr(o), "7,11,-1")
        self.assertEqual(o.length, 3)

        o = run_js("globalThis.o = {a: 7, b: 92}; o")
        self.assertEqual(repr(o), "[object Object]")
        self.assertEqual(o.a, 7)
        self.assertEqual(o.b, 92)
        o.a = 13
        self.assertEqual(run_js("o.a"), 13)

    def test_jsproxy_bool(self):
        self.assertTrue(run_js("new TextDecoder()"))
        self.assertFalse(run_js("[]"))
        self.assertTrue(run_js("[1]"))
        self.assertTrue(run_js("() => {}"))
        self.assertTrue(run_js("(function(){})"))
        self.assertFalse(run_js("new Map()"))
        self.assertTrue(run_js("new Map([[1, 7]])"))
        self.assertFalse(run_js("new Set()"))
        self.assertTrue(run_js("new Set([1, 7])"))

    def test_jsproxy_js_id(self):
        o = run_js("let a = {}; let b = {}; ({a, b, c: a})")
        a = o.a
        b = o.b
        c = o.c
        self.assertEqual(a.js_id, c.js_id)
        self.assertIsNot(a, c)
        self.assertNotEqual(a.js_id, b.js_id)

    def test_jsproxy_compare(self):
        o = run_js("let a = {}; let b = {}; ({a, b, c: a})")
        a = o.a
        b = o.b
        c = o.c
        self.assertEqual(a, c)
        self.assertIsNot(a, c)
        self.assertNotEqual(a, b)
        self.assertNotEqual(a, {})
        with self.assertRaisesRegex(TypeError, "not supported"):
            a < b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a > b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a <= b
        with self.assertRaisesRegex(TypeError, "not supported"):
            a >= b

    def test_jsproxy_to_js(self):
        o = run_js("({x: 3, y: 7})")
        f = run_js("(o) => o.x + o.y")
        self.assertEqual(f(o), 10)

    def test_jsproxy_construct(self):
        URL = run_js("URL")
        with self.assertRaises(JsError):
            URL("http://example.com")
        r = URL.new("http://example.com/a/b?c=2")
        self.assertEqual(r.origin, "http://example.com")
        self.assertEqual(r.protocol, "http:")
        self.assertEqual(r.pathname, "/a/b")
        self.assertEqual(r.search, "?c=2")

    def test_jsproxy_iter(self):
        l = run_js("[9, 32, 12, 17]")
        self.assertEqual(list(l), [9, 32, 12, 17])

    def test_jsproxy_dir(self):
        a = run_js('({ x : 2, y : "9" })')
        b = run_js("(function(){})")
        dira = set(dir(a))
        dirb = set(dir(b))

        jsproxy_items = {
            "__bool__",
            "__class__",
            "__defineGetter__",
            "__defineSetter__",
            "__delattr__",
            "constructor",
            "toString",
            "valueOf",
        }
        a_items = {"x", "y"}
        callable_items = {"__call__", "new"}
        self.assertTrue(dira.issuperset(jsproxy_items))
        self.assertTrue(dira.isdisjoint(callable_items))
        self.assertTrue(dira.issuperset(a_items))
        self.assertTrue(dirb.issuperset(jsproxy_items))
        self.assertTrue(dirb.issuperset(callable_items))
        self.assertTrue(dirb.isdisjoint(a_items))

    def test_jsproxy_error(self):
        def f():
            raise run_js("new TypeError('hi!')")

        try:
            f()
        except JsError as e:
            err = e
        else:
            assert False

        self.assertEqual(err.name, "TypeError")
        self.assertEqual(err.message, "hi!")
        self.assertHasAttr(err, "stack")

    def test_jsproxy_get(self):
        o = run_js("({get(x) {return x + 1;}})")
        self.assertEqual(o[5], 6)
        self.assertEqual(o[77], 78)

    def test_jsproxy_has(self):
        o = run_js("({has(x) {return x === 3 || x === 5;}})")
        self.assertTrue(3 in o)
        self.assertTrue(5 in o)
        self.assertFalse(1 in o)
        self.assertFalse(7 in o)

    def test_jsproxy_includes(self):
        o = run_js("({includes(x) {return x === 3 || x === 5;}})")
        self.assertTrue(3 in o)
        self.assertTrue(5 in o)
        self.assertFalse(1 in o)
        self.assertFalse(7 in o)

    def test_jsproxy_prefer_has_over_includes(self):
        o = run_js(
            "({includes(x) {return x === 3;}, has(x) { return x === 5 }})"
        )
        self.assertTrue(5 in o)
        self.assertFalse(3 in o)

    def test_jsproxy_len(self):
        o = run_js("({length: 7})")
        self.assertEqual(len(o), 7)
        o = run_js("({size: 5})")
        self.assertEqual(len(o), 5)
        # Prefer size over length
        o = run_js("({length: 7, size: 5})")
        self.assertEqual(len(o), 5)
        # Even though functions have a .length in JS, there is a special case
        # that makes them not have a len.
        f = run_js("(function(){})")
        with self.assertRaisesRegex(
            TypeError, "object of type 'pyodide.ffi.JsProxy' has no len()"
        ):
            len(f)

    def test_jsproxy_len_errors(self):
        o = run_js(f"({{length : []}})")
        with self.assertRaises(
            TypeError, msg="object does not have a valid length"
        ):
            len(o)

        for n in [1 << 31, 1 << 32, 1 << 33, 1 << 63, 1 << 64, 1 << 65]:
            o = run_js(f"({{length : {n}}})")
            msg = f"length {n} of object is larger than INT_MAX (2147483647)"
            with self.assertRaises(OverflowError, msg=msg):
                len(o)

        for n in [
            -1,
            -2,
            -3,
            -100,
            -1 << 31,
            -1 << 32,
            -1 << 33,
            -1 << 63,
            -1 << 64,
            -1 << 65,
        ]:
            o = run_js(f"({{length : {n}}})")
            msg = f"length {n} of object is negative"
            with self.assertRaises(ValueError, msg=msg):
                len(o)

    def test_jsproxy_set(self):
        o = run_js("({set(key, val) {this['$' + key] = val;} })")
        o["x"] = 7
        self.assertEqual(run_js("(o) => o.$x")(o), 7)

    def test_jsproxy_map(self):
        o = run_js("new Map([[3, 1], [5, 2]])")
        self.assertEqual(o[3], 1)
        self.assertEqual(o[5], 2)
        o[7] = 9
        del o[3]
        l = set(run_js("(o) => o.keys()")(o))
        self.assertEqual(l, {5, 7})

    def test_array(self):
        pyl = [5, 1, 2, 3]
        jsl = run_js(str(pyl))
        pyl2 = [7, -1, 66]
        jsl2 = run_js(str(pyl2))
        self.assertEqual(jsl[0], pyl[0])
        self.assertEqual(jsl[-1], pyl[-1])
        self.assertEqual(list(jsl), pyl)
        jsl[0] = 25
        pyl[0] = 25
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(list(jsl[1:-2]), pyl[1:-2])
        self.assertEqual(list(jsl[::2]), pyl[::2])
        self.assertEqual(list(reversed(jsl)), list(reversed(pyl)))
        jsl[3:3] = [1, 2, 3]
        pyl[3:3] = [1, 2, 3]
        self.assertEqual(list(jsl), pyl)
        jsl += [7, 6, 5]
        pyl += [7, 6, 5]
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(list(jsl + jsl2), pyl + pyl2)
        self.assertEqual(list(jsl * 2), pyl * 2)
        jsl *= 2
        pyl *= 2
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(len(jsl), len(pyl))

        def gen():
            yield 6
            yield -3
            yield 52

        jsl.extend(gen())
        pyl.extend(gen())
        self.assertEqual(list(jsl), pyl)
        self.assertEqual(jsl.pop(), pyl.pop())
        self.assertEqual(list(jsl), pyl)
        jsl.append(-3)
        pyl.append(-3)
        self.assertEqual(list(jsl), pyl)
        jsl.reverse()
        pyl.reverse()
        self.assertEqual(list(jsl), pyl)
        jsl.insert(5, 22)
        pyl.insert(5, 22)
        self.assertEqual(list(jsl), pyl)
        # TODO:
        # index, count, remove

    def test_gen_close_throw(self):
        f = run_js(
            """
            (function *() {
                try {
                    yield 1;
                } finally {
                    yield 2;
                    console.log("finally");
                }
            })
            """
        )

        g = f()
        assert next(g) == 1
        assert g.throw(TypeError("hi")) == 2
        with self.assertRaises(TypeError, msg="hi"):
            next(g)

        g = f()
        assert next(g) == 1
        assert g.throw(TypeError, "hi") == 2
        with self.assertRaises(TypeError, msg="hi"):
            next(g)

        f = run_js(
            """
            (function *() {
                yield 1;
                yield 2;
                yield 3;
            })
            """
        )
        g = f()
        assert next(g) == 1
        g.close()


class PyProxyTest(TestCase):
    def test_pyproxy(self):
        d = {1: 7}
        self.assertEqual(run_js("(x) => x.toString()")(d), str(d))
        self.assertEqual(run_js("(x) => x.type")(d), "dict")

    def test_pyproxy_to_py(self):
        x = [1, 2, 3]
        self.assertIs(run_js("(x) => x")(x), x)

    def test_pyproxy_call_simple(self):
        def f(x):
            return x * x + 7

        self.assertEqual(run_js("(f) => f(7)")(f), f(7))

    def test_pyproxy_call_kwargs(self):
        def f(*, x, y):
            return x * x + y * y

        self.assertEqual(
            run_js("(f) => f.callKwargs({x: 7, y: 3})")(f), f(x=7, y=3)
        )

    def test_pyproxy_call_full(self):
        def f(x=2, y=3):
            return [x, y]

        self.assertEqual(list(run_js("(f) => f()")(f)), [2, 3])
        self.assertEqual(list(run_js("(f) => f(7)")(f)), [7, 3])
        self.assertEqual(list(run_js("(f) => f(7, -1)")(f)), [7, -1])
        self.assertEqual(list(run_js("(f) => f.callKwargs({})")(f)), [2, 3])
        self.assertEqual(list(run_js("(f) => f.callKwargs(7, {})")(f)), [7, 3])
        self.assertEqual(
            list(run_js("(f) => f.callKwargs(7, -1, {})")(f)), [7, -1]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs({ y : 4 })")(f)), [2, 4]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs({ y : 4, x : 9 })")(f)), [9, 4]
        )
        self.assertEqual(
            list(run_js("(f) => f.callKwargs(8, { y : 4 })")(f)), [8, 4]
        )

        with self.assertRaisesRegex(
            JsError, "TypeError: callKwargs requires at least one argument"
        ):
            run_js("(f) => f.callKwargs()")(f)

        with self.assertRaisesRegex(
            TypeError, r"f\(\) got an unexpected keyword argument 'z'"
        ):
            run_js("(f) => f.callKwargs({z : 6})")(f)

        with self.assertRaisesRegex(
            TypeError, r"f\(\) got multiple values for argument 'x'"
        ):
            run_js("(f) => f.callKwargs(76, {x : 6})")(f)

    def test_pyproxy_this1(self):
        def f(self, x):
            return getattr(self, x)

        res = run_js(
            """
            (f) => {
                const x = {};
                x.f = f.captureThis();
                x.a = 7;
                return x.f("a");
            }
            """
        )(f)
        self.assertEqual(res, 7)

    def test_pyproxy_bind(self):
        self.skipTest("TODO")

    def test_pyproxy_contains(self):
        d = {1, "a"}
        resjs = run_js("(x) => [x.has(1), x.has(2), x.has('a'), x.has('b')]")(
            d
        )
        self.assertEqual(list(resjs), [True, False, True, False])

        with self.assertRaisesRegex(TypeError, "unhashable type"):
            run_js("(x) => x.has({})")(d)

    def test_pyproxy_get(self):
        d = {1: 2, "a": "q"}
        resjs = run_js("(x) => [x.get(1), x.get(2), x.get('a'), x.get('b')]")(
            d
        )
        self.assertEqual(list(resjs), [2, None, "q", None])
