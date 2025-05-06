/* _pyodide_core module: JavaScript ffi */
#include "Python.h"
#include "clinic/_pyodide_core.c.h"
#include "js2python.h"
#include "python2js.h"
#include "jslib.h"
#include "jsproxy.h"


#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "error_handling.h"

PyDoc_STRVAR(_pyodide_core_doc,
"Emscripten-only functionality for JS FFI");

EM_JS_VAL(JsVal, _pyodide_core_get_eval, (void), {
    return eval;
});


/*[clinic input]
module _pyodide_core
_pyodide_core.bad_hiwire_get

    select: 'i'
    /

Run hiwire_get on bad imputs to test the errors.

Currently this fatally crashes the runtime so it can't be used in a test.
[clinic start generated code]*/

static PyObject *
_pyodide_core_bad_hiwire_get_impl(PyObject *module, int select)
/*[clinic end generated code: output=25686670072611be input=50b8fdde919bb598]*/
{
    switch(select) {
        case 1:
            hiwire_get((JsRef)0);
            return NULL;
        case 2:
            PyErr_SetString(PyExc_TypeError, "oops!");
            hiwire_get((JsRef)0);
            return NULL;
        case 3:
            hiwire_get((JsRef)3);
            return NULL;
    }
    return NULL;
}

int
jsproxy_init(PyObject *m);

static int
_pyodide_core_exec(PyObject *m)
{
  bool success = false;

  FAIL_IF_MINUS_ONE(jsproxy_init(m));
  JsVal eval = _pyodide_core_get_eval();
  FAIL_IF_JS_NULL(eval);
  FAIL_IF_MINUS_ONE(
    PyModule_Add(m, "run_js", JsProxy_create(eval)));

  success = true;
finally:
  return success ? 0 : -1;
}

static PyModuleDef_Slot _pyodide_core_slots[] = {
    {Py_mod_exec, _pyodide_core_exec},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
    {0, NULL}
};

static struct PyMethodDef _pyodide_core_functions[] = {
    _PYODIDE_CORE_BAD_HIWIRE_GET_METHODDEF
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
