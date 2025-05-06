#include "jslib.h"
#include "Python.h"

PyObject*
JsProxy_create(JsVal object);

PyObject*
JsProxy_create_with_this(JsVal object,
                         JsVal this);

bool
JsProxy_Check(PyObject* x);
