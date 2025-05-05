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

PyDoc_STRVAR(_pyodide_core_test_js2python__doc__,
"test_js2python($module, arg, /)\n"
"--\n"
"\n"
"A test that we can convert some simple JavaScript values to Python");

#define _PYODIDE_CORE_TEST_JS2PYTHON_METHODDEF    \
    {"test_js2python", (PyCFunction)_pyodide_core_test_js2python, METH_O, _pyodide_core_test_js2python__doc__},

static PyObject *
_pyodide_core_test_js2python_impl(PyObject *module, int arg);

static PyObject *
_pyodide_core_test_js2python(PyObject *module, PyObject *arg_)
{
    PyObject *return_value = NULL;
    int arg;

    arg = PyLong_AsInt(arg_);
    if (arg == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = _pyodide_core_test_js2python_impl(module, arg);

exit:
    return return_value;
}
/*[clinic end generated code: output=7235672407403889 input=a9049054013a1b77]*/
