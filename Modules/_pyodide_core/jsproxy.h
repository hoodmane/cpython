#include "jslib.h"
#include "Python.h"

PyObject*
_PyJsProxy_create(JsVal object);

PyObject*
_PyJsProxy_create_with_this(JsVal object,
                         JsVal this);

bool
_PyJsProxy_Check(PyObject* x);

JsVal
_PyJsProxy_Val(PyObject* x);
