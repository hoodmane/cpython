#include "jslib.h"
#include "Python.h"
#include "pytypedefs.h"

JsVal
pyproxy_new(PyObject* obj);

bool
PyProxy_Check(JsVal);

void
PyProxy_Destroy(JsVal pyproxy, Js_Identifier* msg);
