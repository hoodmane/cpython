#include "jsproxy_call.h"
#include "python2js.h"
#include "js2python.h"
#include "error_handling.h"

/**
 * Prepare arguments from a `METH_FASTCALL | METH_KEYWORDS` Python function to a
 * JavaScript call. We call `python2js` on each argument.
 */
static JsVal
JsMethod_ConvertArgs(PyObject* const* pyargs,
                     Py_ssize_t nargsf,
                     PyObject* kwnames)
{
  JsVal jsargs = _PyJsvArray_New();

  int nargs = PyVectorcall_NARGS(nargsf);
  // present positional arguments
  for (Py_ssize_t i = 0; i < nargs; ++i) {
    JsVal arg = _Py_python2js(pyargs[i]);
    FAIL_IF_JS_ERROR(arg);
    _PyJsvArray_Push(jsargs, arg);
  }
  // Keyword arguments
  // Skip if there are no keyword arguments
  Py_ssize_t nkwargs = kwnames == NULL ? 0 : PyTuple_GET_SIZE(kwnames);
  if (nkwargs == 0) {
    goto success;
  }
  // store kwargs into an object which we'll use as the last argument.
  JsVal kwargs = _PyJsvObject_New();
  FAIL_IF_JS_ERROR(kwargs);
  for (int64_t i = 0, k = nargs; i < nkwargs; ++i, ++k) {
    PyObject* pyname = PyTuple_GET_ITEM(kwnames, i);
    JsVal jsname = _Py_python2js(pyname);
    FAIL_IF_JS_ERROR(jsname);
    JsVal arg = _Py_python2js(pyargs[k]);
    FAIL_IF_JS_ERROR(arg);
    FAIL_IF_MINUS_ONE(_PyJsvObject_SetAttr(kwargs, jsname, arg));
  }
  _PyJsvArray_Push(jsargs, kwargs);

  FAIL_IF_ERR_OCCURRED();
  goto success;

success:
  return jsargs;
finally:
  return JS_ERROR;
}


/**
 * __call__ overload for methods. Controlled by IS_CALLABLE.
 */
PyObject*
_PyJsMethod_Vectorcall_impl(JsVal func,
                         JsVal receiver,
                         PyObject* const* pyargs,
                         size_t nargsf,
                         PyObject* kwnames)
{
  bool success = false;
  JsVal jsresult = JS_ERROR;
  PyObject* pyresult = NULL;

  // Recursion error?
  FAIL_IF_NONZERO(Py_EnterRecursiveCall(" while calling a JavaScript object"));
  JsVal jsargs =
    JsMethod_ConvertArgs(pyargs, nargsf, kwnames);
  FAIL_IF_JS_ERROR(jsargs);
  jsresult = _PyJsvFunction_CallBound(func, receiver, jsargs);
  FAIL_IF_JS_ERROR(jsresult);
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);

  success = true;
finally:
  Py_LeaveRecursiveCall(/* " in JsMethod_Vectorcall" */);
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}

PyObject*
_PyJsMethod_Construct_impl(JsVal func,
                        PyObject* const* pyargs,
                        size_t nargs,
                        PyObject* kwnames)
{
  bool success = false;
  PyObject* pyresult = NULL;

  // Recursion error?
  FAIL_IF_NONZERO(Py_EnterRecursiveCall(" in JsMethod_Construct"));

  JsVal jsargs = JsMethod_ConvertArgs(pyargs, nargs, kwnames);
  FAIL_IF_JS_ERROR(jsargs);
  JsVal jsresult = _PyJsvFunction_Construct(func, jsargs);
  FAIL_IF_JS_ERROR(jsresult);
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);

  success = true;
finally:
  Py_LeaveRecursiveCall(/* " in JsMethod_Construct" */);
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}
