#include "jslib.h"
#include "Python.h"
#include "pytypedefs.h"

JsVal
_PyProxy_New(PyObject* obj);

JsVal
_PyProxy_NewEx(PyObject* obj,
               bool capture_this,
               bool roundtrip,
               bool register);

int
_PyProxy_Check(JsVal);

void
_PyProxy_Destroy(JsVal pyproxy, Js_Identifier* msg);

/**
 * If x is a PyProxy, return a borrowed version of the wrapped PyObject. Returns
 * NULL if x is NULL or a valid JsRef which is not a pyproxy. Fatally fails if x
 * is not NULL or a valid JsRef.
 */
PyObject*
_PyProxy_AsPyObject(JsVal x);
