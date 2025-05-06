from unittest import TestCase
from _pyodide_core import test_error_handling, test_js2python, test_python2js, run_js

class PyodideTest(TestCase):
    def test_error_handling(self):
        self.assertEqual(test_error_handling(0), 8)
        with self.assertRaisesRegex(RuntimeError, "Error: Hi!"):
            test_error_handling(1)
    
    def  test_js2python(self):
        with self.assertRaisesRegex(RuntimeError, "Error: hi!"):
            test_js2python(-1)
        self.assertEqual(test_js2python(1), 7)
        self.assertEqual(test_js2python(2), 2.3)
        self.assertEqual(test_js2python(3), 77015781075109876017131518)
        self.assertEqual(test_js2python(4), "abc")
        self.assertIs(test_js2python(5), None)
        self.assertIs(test_js2python(6), False)
        self.assertIs(test_js2python(7), True)
        self.assertEqual(test_js2python(9), "pyodidé")
        self.assertEqual(test_js2python(10), "碘化物")
        self.assertEqual(test_js2python(11), "🐍")

    def  test_python2js(self):
        with self.assertRaisesRegex(TypeError, r"No JavaScript conversion known for Python object: \[\]\."):
            test_python2js(-1, [])
        test_python2js(1, None)
        test_python2js(2, True)
        test_python2js(3, False)
        test_python2js(4, 173)
        test_python2js(5, 23.75)
        test_python2js(6, 77015781075109876017131518)
        test_python2js(7, "abc")
        test_python2js(8, "pyodidé")
        test_python2js(9, "碘化物")
        test_python2js(10, "🐍")

    def test_run_js(self):
        self.assertEqual(run_js("3 + 4"), 7)
        self.assertEqual(run_js('"pyodidé"'), "pyodidé")
        self.assertEqual(run_js('"碘化物"'), "碘化物")
        self.assertEqual(run_js('"🐍"'), "🐍")
        self.assertEqual(run_js('2.3'), 2.3)
        self.assertEqual(run_js('77015781075109876017131518n'), 77015781075109876017131518)
        self.assertEqual(run_js('undefined'), None)
        self.assertEqual(run_js('null'), None)
        self.assertEqual(run_js('false'), False)
        self.assertEqual(run_js('true'), True)
        with self.assertRaisesRegex(RuntimeError, "TypeError: oops!"):
            run_js('throw new TypeError("oops!")')

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
