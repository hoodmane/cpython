from unittest import TestCase
from _pyodide_core import run_js

class PyodideTest(TestCase):
    def test_run_js(self):
        self.assertEqual(run_js("3 + 4"), 7)
        with self.assertRaisesRegex(RuntimeError, "Error: Hi!"):
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
