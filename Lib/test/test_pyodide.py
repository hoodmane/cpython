from unittest import TestCase
from _pyodide_core import test_error_handling, test_js2python

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
        with self.assertRaisesRegex(TypeError, "No Python conversion known for JavaScript object of type Array"):
            test_js2python(8)
