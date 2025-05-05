/* _pyodide_core module: JavaScript ffi */
#include "Python.h"
#include "clinic/_pyodide_core.c.h"
#include "js2python.h"
#include "jslib.h"


#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "error_handling.h"

PyDoc_STRVAR(_pyodide_core_doc,
"Emscripten-only functionality for JS FFI");


EM_JS_NUM(int, test_error_handling_js, (int arg), {
    if (arg === 1) {
        throw new Error("Hi!");
    } else {
        return 8;
    }
});


/*[clinic input]
module _pyodide_core
_pyodide_core.test_error_handling

    arg: 'p'
        A boolean
    /

A basic test that we can handle JavaScript errors
[clinic start generated code]*/

static PyObject *
_pyodide_core_test_error_handling_impl(PyObject *module, int arg)
/*[clinic end generated code: output=29344ab5b91a87a5 input=ddf826749627959a]*/
{
    int res = test_error_handling_js(arg);
    if (res == -1) {
        return NULL;
    }
    return PyLong_FromLong(res);
}



EM_JS_VAL(JsVal, test_js2python_js, (int arg), {
    switch (arg) {
        case 1:
            return 7;
        case 2:
            return 2.3;
        case 3:
            return 77015781075109876017131518n;
        case 4:
            return "abc";
        case 5:
            return undefined;
        case 6:
            return false;
        case 7:
            return true;
    }
    throw new Error("hi!");
});


/*[clinic input]
_pyodide_core.test_js2python

    arg: 'i'
    /

A test that we can convert some simple JavaScript values to Python
[clinic start generated code]*/

static PyObject *
_pyodide_core_test_js2python_impl(PyObject *module, int arg)
/*[clinic end generated code: output=8b0ef786c7ae0eb7 input=8d4e771be43453c7]*/
{
    JsVal res = test_js2python_js(arg);
    if (JsvNull_Check(res)) {
        return NULL;
    }
    return js2python(res);
}



static PyModuleDef_Slot _pyodide_core_slots[] = {
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static struct PyMethodDef _pyodide_core_functions[] = {
    _PYODIDE_CORE_TEST_ERROR_HANDLING_METHODDEF
    _PYODIDE_CORE_TEST_JS2PYTHON_METHODDEF
    {NULL,       NULL}          /* sentinel */
};

static struct PyModuleDef _pyodide_core_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_pyodide_core",
    .m_doc = _pyodide_core_doc,
    .m_methods = _pyodide_core_functions,
    .m_slots = _pyodide_core_slots,
};

PyMODINIT_FUNC
PyInit__pyodide_core(void)
{
    return PyModuleDef_Init(&_pyodide_core_module);
}
