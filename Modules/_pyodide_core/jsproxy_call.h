#include "Python.h"
#include "jslib.h"

PyObject*
_PyJsMethod_Vectorcall_impl(JsVal func,
                         JsVal receiver,
                         PyObject* const* pyargs,
                         size_t nargsf,
                         PyObject* kwnames);

PyObject*
_PyJsMethod_Construct_impl(JsVal func,
                        PyObject* const* pyargs,
                        size_t nargs,
                        PyObject* kwnames);
