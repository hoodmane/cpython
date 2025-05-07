#include "Python.h"
#include "python2js.h"
#include "emscripten.h"
#include "error_handling.h"


EM_JS_VAL(JsVal, pyproxy_new, (PyObject * ptrobj), {
  return Module.pyproxy_new(ptrobj);
});

EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_str(PyObject* pyobj)
{
  PyObject* pystr = NULL;
  JsVal jsrepr = JS_NULL;

  pystr = PyObject_Str(pyobj);
  FAIL_IF_NULL(pystr);
  jsrepr = python2js(pystr);

finally:
  Py_CLEAR(pystr);
  return jsrepr;
}

/**
 * Wrapper for the "proxy.type" getter, which behaves a little bit like
 * `type(obj)`, but instead of returning the class we just return the name of
 * the class. The exact behavior is that this usually gives "module.name" but
 * for builtins just gives "name". So in particular, usually it is equivalent
 * to:
 *
 * `type(x).__module__ + "." + type(x).__name__`
 *
 * But other times it behaves like:
 *
 * `type(x).__name__`
 */
EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_type(PyObject* ptrobj)
{
  return JsvUTF8ToString(Py_TYPE(ptrobj)->tp_name);
}


#define Py_ENTER()
#define Py_EXIT()

