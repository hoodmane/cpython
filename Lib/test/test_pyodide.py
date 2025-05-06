from unittest import TestCase
from _pyodide_core import run_js

JsError = type(run_js("new Error()"))

class PyodideTest(TestCase):
    def test_run_js(self):
        self.assertEqual(run_js("3 + 4"), 7)
        with self.assertRaisesRegex(JsError, "Error: Hi!"):
            run_js("throw new Error('Hi!')")

    def test_jsproxy_call(self):
        res = run_js("(x, y) => x + y")(1, 9)
        self.assertEqual(res, 10)

    def  test_python2js(self):
        self.assertTrue(run_js("(x) => x === 7")(7))
        self.assertTrue(run_js("(x) => x === 2.3")(2.3))
        self.assertTrue(run_js("(x) => x === 77015781075109876017131518n")(77015781075109876017131518))
        self.assertTrue(run_js("(x) => x === 'abc'")("abc"))
        self.assertTrue(run_js("(x) => x === undefined")(None))
        self.assertTrue(run_js("(x) => x === false")(False))
        self.assertTrue(run_js("(x) => x === true")(True))
        self.assertTrue(run_js("(x) => x === 'pyodidé'")("pyodidé"))
        self.assertTrue(run_js("(x) => x === '碘化物'")("碘化物"))
        self.assertTrue(run_js("(x) => x === '🐍'")("🐍"))

    def  test_js2python(self):
        self.assertEqual(run_js('"pyodidé"'), "pyodidé")
        self.assertEqual(run_js('"碘化物"'), "碘化物")
        self.assertEqual(run_js('"🐍"'), "🐍")
        self.assertEqual(run_js('2.3'), 2.3)
        self.assertEqual(run_js('77015781075109876017131518n'), 77015781075109876017131518)
        self.assertEqual(run_js('undefined'), None)
        self.assertEqual(run_js('null'), None)
        self.assertEqual(run_js('false'), False)
        self.assertEqual(run_js('true'), True)

    def test_jsproxy(self):
        o = run_js('[7, 11, -1]')
        self.assertEqual(repr(o), '7,11,-1')
        self.assertEqual(o.length, 3)

        o = run_js('globalThis.o = {a: 7, b: 92}; o')
        self.assertEqual(repr(o), '[object Object]')
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

