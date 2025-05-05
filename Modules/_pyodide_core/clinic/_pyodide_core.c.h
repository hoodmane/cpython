/*[clinic input]
preserve
[clinic start generated code]*/

#include "pycore_modsupport.h"    // _PyArg_CheckPositional()

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

PyDoc_STRVAR(_pyodide_core_test_python2js__doc__,
"test_python2js($module, test, arg, /)\n"
"--\n"
"\n"
"A test that we can convert some simple Python values to JavaScript");

#define _PYODIDE_CORE_TEST_PYTHON2JS_METHODDEF    \
    {"test_python2js", _PyCFunction_CAST(_pyodide_core_test_python2js), METH_FASTCALL, _pyodide_core_test_python2js__doc__},

static PyObject *
_pyodide_core_test_python2js_impl(PyObject *module, int test, PyObject *arg);

static PyObject *
_pyodide_core_test_python2js(PyObject *module, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    int test;
    PyObject *arg;

    if (!_PyArg_CheckPositional("test_python2js", nargs, 2, 2)) {
        goto exit;
    }
    test = PyLong_AsInt(args[0]);
    if (test == -1 && PyErr_Occurred()) {
        goto exit;
    }
    arg = args[1];
    return_value = _pyodide_core_test_python2js_impl(module, test, arg);

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

PyDoc_STRVAR(_pyodide_core_run_js__doc__,
"run_js($module, js_code, /)\n"
"--\n"
"\n"
"Run some JavaScript code and return the result");

#define _PYODIDE_CORE_RUN_JS_METHODDEF    \
    {"run_js", (PyCFunction)_pyodide_core_run_js, METH_O, _pyodide_core_run_js__doc__},

static PyObject *
_pyodide_core_run_js_impl(PyObject *module, const char *js_code);

static PyObject *
_pyodide_core_run_js(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    const char *js_code;

    if (!PyUnicode_Check(arg)) {
        _PyArg_BadArgument("run_js", "argument", "str", arg);
        goto exit;
    }
    Py_ssize_t js_code_length;
    js_code = PyUnicode_AsUTF8AndSize(arg, &js_code_length);
    if (js_code == NULL) {
        goto exit;
    }
    if (strlen(js_code) != (size_t)js_code_length) {
        PyErr_SetString(PyExc_ValueError, "embedded null character");
        goto exit;
    }
    return_value = _pyodide_core_run_js_impl(module, js_code);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_bad_hiwire_get__doc__,
"bad_hiwire_get($module, select, /)\n"
"--\n"
"\n"
"Run hiwire_get on bad imputs to test the errors.\n"
"\n"
"Currently this fatally crashes the runtime so it can\'t be used in a test.");

#define _PYODIDE_CORE_BAD_HIWIRE_GET_METHODDEF    \
    {"bad_hiwire_get", (PyCFunction)_pyodide_core_bad_hiwire_get, METH_O, _pyodide_core_bad_hiwire_get__doc__},

static PyObject *
_pyodide_core_bad_hiwire_get_impl(PyObject *module, int select);

static PyObject *
_pyodide_core_bad_hiwire_get(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int select;

    select = PyLong_AsInt(arg);
    if (select == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = _pyodide_core_bad_hiwire_get_impl(module, select);

exit:
    return return_value;
}
/*[clinic end generated code: output=83ac051fac7b70b9 input=a9049054013a1b77]*/
