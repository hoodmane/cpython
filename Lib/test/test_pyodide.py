from unittest import TestCase
from _pyodide_core import test_error_handling

class PyodideTest(TestCase):
    def test_error_handling(self):
        self.assertEqual(test_error_handling(0), 8)
        with self.assertRaisesRegex(RuntimeError, "Error: Hi!"):
            test_error_handling(1)
