/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_pyodide_core_test_error_handling__doc__,
"test_error_handling($module, arg, /)\n"
"--\n"
"\n"
"A basic test that we can handle JavaScript errors\n"
"\n"
"  arg\n"
"    A boolean");

#define _PYODIDE_CORE_TEST_ERROR_HANDLING_METHODDEF    \
    {"test_error_handling", (PyCFunction)_pyodide_core_test_error_handling, METH_O, _pyodide_core_test_error_handling__doc__},

static PyObject *
_pyodide_core_test_error_handling_impl(PyObject *module, int arg);

static PyObject *
_pyodide_core_test_error_handling(PyObject *module, PyObject *arg_)
{
    PyObject *return_value = NULL;
    int arg;

    arg = PyObject_IsTrue(arg_);
    if (arg < 0) {
        goto exit;
    }
    return_value = _pyodide_core_test_error_handling_impl(module, arg);

exit:
    return return_value;
}
/*[clinic end generated code: output=127f839cf84e4dc3 input=a9049054013a1b77]*/
