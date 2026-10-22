#include "jslib.h"
#include "Python.h"
#include "pytypedefs.h"

JsVal
_PyProxy_New(PyObject* obj);

JsVal
_PyProxy_NewEx(PyObject* obj,
               bool capture_this,
               bool roundtrip,
               bool register,
               bool is_json_adaptor);

int
_PyProxy_Check(JsVal);

void
_PyProxy_Destroy(JsVal pyproxy, Js_Identifier* msg);

void
_Py_gc_register_pyproxies(JsVal pyproxies);

/**
 * If x is a PyProxy, return a borrowed version of the wrapped PyObject. Returns
 * NULL if x is NULL or a valid JsRef which is not a pyproxy. Fatally fails if x
 * is not NULL or a valid JsRef.
 */
PyObject*
_PyProxy_AsPyObject(JsVal x);

/**
 * Create a pair [onFulfilled, onRejected] of JavaScript functions suitable for
 * passing to Promise.then. handle_result and handle_exception are Python
 * callables (either may be NULL). Both are released when either handler is
 * called. done_callback is a JavaScript function or null; it is called with
 * the result (or undefined) after the handler runs.
 */
JsVal
_PyProxy_create_promise_handles(PyObject* handle_result,
                                PyObject* handle_exception,
                                JsVal done_callback);

/**
 * Wrap a Python callable in a JavaScript function that may be called exactly
 * once. The reference to the Python callable is released when the function is
 * called or when the JavaScript function is garbage collected.
 */
JsVal
_PyProxy_create_once_callable(PyObject* obj);
