#include "jslib.h"
#include "Python.h"

JsVal
pyproxy_new(PyObject* obj);

bool
PyProxy_Check(JsVal);
