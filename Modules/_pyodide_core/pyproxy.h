#include "jslib.h"
#include "Python.h"
#include "pytypedefs.h"

JsVal
pyproxy_new(PyObject* obj);

JsVal
pyproxy_new_ex(PyObject* obj,
               bool capture_this,
               bool roundtrip,
               bool register);

bool
PyProxy_Check(JsVal);

void
PyProxy_Destroy(JsVal pyproxy, Js_Identifier* msg);
