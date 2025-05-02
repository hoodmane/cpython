/* _pyodide_core module: JavaScript ffi */
#include "Python.h"
#include "clinic/_pyodide_core.c.h"


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


static PyModuleDef_Slot _pyodide_core_slots[] = {
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static struct PyMethodDef _pyodide_core_functions[] = {
    _PYODIDE_CORE_TEST_ERROR_HANDLING_METHODDEF
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
