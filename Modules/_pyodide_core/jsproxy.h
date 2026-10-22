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

/**
 * Create a Future attached to the given Promise. When the promise is
 * resolved/rejected, the status of the future is set accordingly and
 * done_callback (if not null) is called.
 */
PyObject*
_PyJsProxy_wrap_promise(JsVal promise, JsVal done_callback);
