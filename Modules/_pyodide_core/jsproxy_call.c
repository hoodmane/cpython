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
  JsVal kwargs;
  JsVal jsargs = JsvArray_New();
  bool success = false;

  int nargs = PyVectorcall_NARGS(nargsf);
  // present positional arguments
  for (Py_ssize_t i = 0; i < nargs; ++i) {
    JsVal arg = python2js(pyargs[i]);
    FAIL_IF_JS_NULL(arg);
    JsvArray_Push(jsargs, arg);
  }
  // Keyword arguments
  // Can skip if there are no keyword arguments
  Py_ssize_t nkwargs = kwnames == NULL ? 0 : PyTuple_GET_SIZE(kwnames);
  bool has_kwargs = (nkwargs > 0);
  if (!has_kwargs) {
    goto success;
  }
  // store kwargs into an object which we'll use as the last argument.
  kwargs = JsvObject_New();
  FAIL_IF_JS_NULL(kwargs);
  for (int64_t i = 0, k = nargs; i < nkwargs; ++i, ++k) {
    PyObject* pyname = PyTuple_GET_ITEM(kwnames, i); /* borrowed! */
    JsVal jsname = python2js(pyname);
    FAIL_IF_JS_NULL(jsname);
    JsVal arg = python2js(pyargs[k]);
    FAIL_IF_JS_NULL(arg);
    FAIL_IF_MINUS_ONE(JsvObject_SetAttr(kwargs, jsname, arg));
  }
  JsvArray_Push(jsargs, kwargs);

  FAIL_IF_ERR_OCCURRED();
  goto success;

success:
  success = true;
finally:
  if (!success) {
    jsargs = JS_NULL;
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_SystemError, "Oops");
    }
  }
  return jsargs;
}


/**
 * __call__ overload for methods. Controlled by IS_CALLABLE.
 */
PyObject*
JsMethod_Vectorcall_impl(JsVal func,
                         JsVal receiver,
                         PyObject* const* pyargs,
                         size_t nargsf,
                         PyObject* kwnames)
{
  bool success = false;
  JsVal jsresult = JS_NULL;
  PyObject* pyresult = NULL;

  // Recursion error?
  FAIL_IF_NONZERO(Py_EnterRecursiveCall(" while calling a JavaScript object"));
  JsVal jsargs =
    JsMethod_ConvertArgs(pyargs, nargsf, kwnames);
  FAIL_IF_JS_NULL(jsargs);
  jsresult = JsvFunction_CallBound(func, receiver, jsargs);
  FAIL_IF_JS_NULL(jsresult);
  pyresult = js2python(jsresult);
  FAIL_IF_NULL(pyresult);

  success = true;
finally:
  Py_LeaveRecursiveCall(/* " in JsMethod_Vectorcall" */);
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}
