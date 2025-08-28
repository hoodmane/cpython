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
    if (e instanceof PythonError) {
      // Try to restore the original Python exception.
      const restored_error = _restore_sys_last_exception(e.__error_address);
      if (restored_error) {
        return;
      }
    }
    let err = _JsProxy_create(e);
    _set_error(err);
    _Py_DecRef(err);
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

EM_JS(void, capture_stderr, (void), {
  FS.createDevice("/dev", "capture_stderr", null, (e) =>
    stderr_chars.push(e),
  );
  FS.closeStream(2 /* stderr */);
  // open takes the lowest available file descriptor. Since 0 and 1 are occupied by stdin and stdout it takes 2.
  FS.open("/dev/capture_stderr", 1 /* O_WRONLY */);
}
const stderr_chars = [];
);

EM_JS(JsVal, restore_stderr, (void), {
  FS.closeStream(2 /* stderr */);
  FS.unlink("/dev/capture_stderr");
  // open takes the lowest available file descriptor. Since 0 and 1 are occupied by stdin and stdout it takes 2.
  FS.open("/dev/stderr", 1 /* O_WRONLY */);
  return UTF8ArrayToString(new Uint8Array(stderr_chars));
})


EM_JS(
JsVal,
new_error,
(const char* type, JsVal msg, PyObject* err),
{
  return new PythonError(UTF8ToString(type), msg, err);
});

/**
 * Restore sys.last_exception as the current exception if sys.last_exc matches
 * the argument `exc`. Used for reentrant errors.
 * Returns true if it restored the error indicator, false otherwise.
 *
 * If we throw a JavaScript PythonError and it bubbles out to the enclosing
 * Python scope (i.e., doesn't get caught in JavaScript) then we want to restore
 * the original Python exception. This produces much better stack traces in case
 * of reentrant calls and prevents issues like a KeyboardInterrupt being wrapped
 * into a PythonError being wrapped into a JsException and being caught.
 *
 * We don't do the same thing for JavaScript messages that pass through Python
 * because the Python exceptions have good JavaScript stack traces but
 * JavaScript errors have no Python stack info. Also, JavaScript has much weaker
 * support for catching errors by type.
 */
EMSCRIPTEN_KEEPALIVE bool
restore_sys_last_exception(void* exc)
{
  if (exc == NULL) {
    return false;
  }
  // PySys_GetObject returns a borrowed reference and will return NULL without
  // setting an exception if it fails.
  PyObject* last_exc = PySys_GetObject("last_exc");
  if (last_exc != exc) {
    return false;
  }
  // PyErr_SetRaisedException steals a reference to its argument and
  // PySys_GetObject returns a borrow so need to incref last_xxc first.
  Py_INCREF(last_exc);
  PyErr_SetRaisedException(last_exc);
  return true;
}

EMSCRIPTEN_KEEPALIVE JsVal
wrap_exception(void)
{
  PyObject* typestr = NULL;
  PyObject* exc = NULL;
  JsVal jserror = JS_ERROR;

  exc = PyErr_GetRaisedException();

  capture_stderr();
  PyErr_SetRaisedException(Py_NewRef(exc));
  // print standard traceback to standard error, clear the error flag, and set
  // sys.last_exc, sys.last_type, etc
  PyErr_Print();
  JsVal formatted_exception = restore_stderr();

  typestr = PyObject_GetAttrString((PyObject*)Py_TYPE(exc), "__qualname__");
  FAIL_IF_NULL(typestr);
  const char* typestr_utf8 = PyUnicode_AsUTF8(typestr);
  FAIL_IF_NULL(typestr_utf8);

  jserror = new_error(typestr_utf8, formatted_exception, exc);

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
