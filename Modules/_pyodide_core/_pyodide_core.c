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



EM_JS_NUM(int, test_python2js_js, (int test, JsVal x), {
    const msg = `x: ${x}`;
    switch (test) {
        case 1:
            jsAssert(() => x === undefined, msg);
            return;
        case 2:
            jsAssert(() => x === true, msg);
            return;
        case 3:
            jsAssert(() => x === false, msg);
            return;
        case 4:
            jsAssert(() => x === 173, msg);
            return;
        case 5:
            jsAssert(() => x === 23.75, msg);
            return;
        case 6:
            jsAssert(() => x === 77015781075109876017131518n, msg);
            return;
        case 7:
            jsAssert(() => x === "abc", msg);
            return;
        case 8:
            jsAssert(() => x === "pyodidé", msg);
            return;
        case 9:
            jsAssert(() => x === "碘化物", msg);
            return;
        case 10:
            jsAssert(() => x === "🐍", msg);
            return;
    }
    throw new Error("hi!");
});

/*[clinic input]
_pyodide_core.test_python2js

    test: 'i'
    arg: 'O'
    /

A test that we can convert some simple Python values to JavaScript
[clinic start generated code]*/

static PyObject *
_pyodide_core_test_python2js_impl(PyObject *module, int test, PyObject *arg)
/*[clinic end generated code: output=63ab86c2936f1b9a input=cda2d14193bb0404]*/
{
    JsVal res = python2js(arg);
    if (JsvNull_Check(res)) {
        return NULL;
    }
    int status = test_python2js_js(test, res);
    if (status == 0) {
        Py_RETURN_NONE;
    }
    return NULL;
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
        case 8:
            return [];
        case 9:
            // ucs1
            return "pyodidé";
        case 10:
            // ucs2
            return "碘化物";
        case 11:
            // ucs4
            return "🐍";
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

EM_JS(void, _test_helpers, (void), {}
function jsAssert(cb, message = "") {
    if (message !== "") {
        message = "\\n" + message;
    }
    let cond = cb.toString();
    if (cond.startsWith("()=>")) {
        cond = cond.slice("()=>".length);
    }
    if (cb() !== true) {
        throw new Error(
            `Assertion failed: ${cond}${message}`
        );
    }
});


EM_JS_VAL(JsVal, _pyodide_core_get_eval, (void), {
    return eval;
});


/*[clinic input]
_pyodide_core.bad_hiwire_get

    select: 'i'
    /

Run hiwire_get on bad imputs to test the errors.

Currently this fatally crashes the runtime so it can't be used in a test.
[clinic start generated code]*/

static PyObject *
_pyodide_core_bad_hiwire_get_impl(PyObject *module, int select)
/*[clinic end generated code: output=25686670072611be input=6b09a9ac032a469f]*/
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
    _PYODIDE_CORE_TEST_ERROR_HANDLING_METHODDEF
    _PYODIDE_CORE_TEST_PYTHON2JS_METHODDEF
    _PYODIDE_CORE_TEST_JS2PYTHON_METHODDEF
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
