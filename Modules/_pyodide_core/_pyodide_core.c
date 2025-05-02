/* _pyodide_core module: JavaScript ffi */

#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"

PyDoc_STRVAR(_pyodide_core_doc,
"Emscripten-only functionality for JS FFI");


static PyModuleDef_Slot _pyodide_core_slots[] = {
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static struct PyMethodDef _pyodide_core_functions[] = {
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
