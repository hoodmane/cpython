#include "Python.h"
#include "jslib.h"

PyObject*
JsMethod_Vectorcall_impl(JsVal func,
                         JsVal receiver,
                         PyObject* const* pyargs,
                         size_t nargsf,
                         PyObject* kwnames);
