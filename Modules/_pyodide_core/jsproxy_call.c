#include "jsproxy_call.h"
#include "python2js.h"
#include "js2python.h"
#include "error_handling.h"
#include "pyproxy.h"

/**
 * Prepare arguments from a `METH_FASTCALL | METH_KEYWORDS` Python function to a
 * JavaScript call. We call `python2js` on each argument.
 */
static JsVal
JsMethod_ConvertArgs(PyObject* const* pyargs,
                     Py_ssize_t nargsf,
                     PyObject* kwnames,
                     JsVal proxies)
{
  JsVal jsargs = _PyJsvArray_New();

  int nargs = PyVectorcall_NARGS(nargsf);
  // present positional arguments
  for (Py_ssize_t i = 0; i < nargs; ++i) {
    JsVal arg = _Py_python2js_track_proxies(pyargs[i], proxies, false);
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
    JsVal arg = _Py_python2js_track_proxies(pyargs[k], proxies, false);
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

Js_static_string(PYPROXY_DESTROYED_AT_END_OF_FUNCTION_CALL,
                 "This borrowed proxy was automatically destroyed at the "
                 "end of a function call. Try using "
                 "create_proxy or create_once_callable.");

EM_JS(void, _Py_destroy_proxies, (JsVal proxies, Js_Identifier* msg_ptr), {
  let msg = undefined;
  if (msg_ptr) {
    msg = __PyJsvString_FromId(msg_ptr);
  }
  for (let px of proxies) {
    Module.pyproxy_destroy(px, msg, false);
  }
});

static void
destroy_proxies(JsVal jsval, JsVal proxies)
{
  if (!_PyJsvError_Check(jsval) && _PyProxy_Check(jsval)) {
    // TODO: don't destroy proxies with roundtrip = true?
    _PyJsvArray_Push(proxies, jsval);
  }
  _Py_destroy_proxies(proxies, &PYPROXY_DESTROYED_AT_END_OF_FUNCTION_CALL);
}

EM_JS_VAL(JsVal, _Py_wrap_generator, (JsVal gen, JsVal proxies), {
  proxies = new Set(proxies);
  const msg =
    "This borrowed proxy was automatically destroyed " +
    "when a generator completed execution. Try " +
    "using create_proxy or create_once_callable.";
  function cleanup() {
    proxies.forEach((px) => Module.pyproxy_destroy(px, msg));
  }
  function wrap(funcname) {
    return function (val) {
      if(API.isPyProxy(val)) {
        val = val.copy();
        proxies.add(val);
      }
      let res;
      try {
        res = gen[funcname](val);
      } catch (e) {
        cleanup();
        throw e;
      }
      if (res.done) {
        // Don't destroy the return value!
        proxies.delete(res.value);
        cleanup();
      }
      return res;
    };
  }
  return {
    get [Symbol.toStringTag]() {
      return "Generator";
    },
    [Symbol.iterator]() {
      return this;
    },
    next: wrap("next"),
    throw: wrap("throw"),
    return: wrap("return"),
  };
});

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
  JsVal pyproxies = _PyJsvArray_New();
  bool destroy_args = true;

  // Recursion error?
  FAIL_IF_NONZERO(Py_EnterRecursiveCall(" while calling a JavaScript object"));
  JsVal jsargs =
    JsMethod_ConvertArgs(pyargs, nargsf, kwnames, pyproxies);
  FAIL_IF_JS_ERROR(jsargs);
  jsresult = _PyJsvFunction_CallBound(func, receiver, jsargs);
  FAIL_IF_JS_ERROR(jsresult);
  bool is_generator = _PyJsvGenerator_Check(jsresult);
  destroy_args = !is_generator;
  if (is_generator) {
    jsresult = _Py_wrap_generator(jsresult, pyproxies);
  }
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);


  success = true;
finally:
  Py_LeaveRecursiveCall(/* " in JsMethod_Vectorcall" */);
  if (!success) {
    Py_CLEAR(pyresult);
  }
  if (destroy_args) {
    destroy_proxies(jsresult, pyproxies);
  } else {
    _Py_gc_register_pyproxies(pyproxies);
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
  JsVal proxies = _PyJsvArray_New();

  // Recursion error?
  FAIL_IF_NONZERO(Py_EnterRecursiveCall(" in JsMethod_Construct"));

  JsVal jsargs = JsMethod_ConvertArgs(pyargs, nargs, kwnames, proxies);
  FAIL_IF_JS_ERROR(jsargs);
  JsVal jsresult = _PyJsvFunction_Construct(func, jsargs);
  FAIL_IF_JS_ERROR(jsresult);
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);

  success = true;
finally:
  Py_LeaveRecursiveCall(/* " in JsMethod_Construct" */);
  destroy_proxies(jsresult, proxies);
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}
