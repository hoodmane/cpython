#include "jslib.h"
#include "Python.h"

PyObject*
JsProxy_create(JsVal object);

bool
JsProxy_Check(PyObject* x);
