#include "Python.h"
#include "error_handling.h"
#include <emscripten.h>

/**
 * Set Python error indicator from JavaScript.
 */
EMSCRIPTEN_KEEPALIVE void
set_error(PyObject* err)
{
  PyErr_SetObject((PyObject*)Py_TYPE(err), err);
}

EM_JS(void, error_handling_init_js, (void), {
  Module.handle_js_error = function (e) {
    let err = _JsProxy_create(e);
    _set_error(err);
  };
});

__attribute__((constructor)) void error_handling_init(void) {
  error_handling_init_js();
}

#ifdef DEBUG_F
EM_JS(void, console_error, (char* msg), {
  let jsmsg = UTF8ToString(msg);
  console.error(jsmsg);
});
#endif

EM_JS(
JsVal,
new_error,
(const char* type),
{
  return new Error("A Python error of type " + UTF8ToString(type) + " was raised...");
});

EMSCRIPTEN_KEEPALIVE JsVal
wrap_exception(void)
{
  bool success = false;
  PyObject* typestr = NULL;
  PyObject* exc = NULL;
  JsVal jserror = JS_NULL;

  exc = PyErr_GetRaisedException();
  PyErr_SetRaisedException(Py_NewRef(exc));
  PyErr_Print();

  typestr = PyObject_GetAttrString((PyObject*)Py_TYPE(exc), "___qualname__");
  FAIL_IF_NULL(typestr);
  const char* typestr_utf8 = PyUnicode_AsUTF8(typestr);
  FAIL_IF_NULL(typestr_utf8);

  jserror = new_error(typestr_utf8);

finally:
  Py_CLEAR(typestr);
  Py_CLEAR(exc);
  return jserror;
}


/**
 * Convert the current Python error to a javascript error and throw it.
 */
EMSCRIPTEN_KEEPALIVE void _Py_NO_RETURN
pythonexc2js(void)
{
  JsVal jserror = wrap_exception();
  JsvError_Throw(jserror);
}
