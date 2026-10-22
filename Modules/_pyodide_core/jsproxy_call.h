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

extern Js_Identifier _Py_PYPROXY_DESTROYED_AT_END_OF_FUNCTION_CALL;

/**
 * Destroy each PyProxy in the JavaScript array proxies, with error message
 * msg.
 */
void
_Py_destroy_proxies(JsVal proxies, Js_Identifier* msg);
