#define PY_SSIZE_T_CLEAN
#include "Python.h"

#include "error_handling.h"
#include "js2python.h"

#include <emscripten.h>


// PyUnicodeDATA is a macro, we need to access it from JavaScript
EMSCRIPTEN_KEEPALIVE void*
PyUnicode_Data(PyObject* obj)
{
  return PyUnicode_DATA(obj);
}

EMSCRIPTEN_KEEPALIVE PyObject*
_js2python_none(void)
{
  Py_RETURN_NONE;
}

EMSCRIPTEN_KEEPALIVE PyObject*
_js2python_true(void)
{
  Py_RETURN_TRUE;
}

EMSCRIPTEN_KEEPALIVE PyObject*
_js2python_false(void)
{
  Py_RETURN_FALSE;
}

EM_JS_REF(PyObject*, js2python_js, (JsVal value), {
  let result = Module.js2python_convertImmutable(value);
  // clang-format off
  if (result !== undefined) {
    // clang-format on
    return result;
  }
  return 0;
})

EMSCRIPTEN_KEEPALIVE PyObject*
js2python(JsVal val)
{
  return js2python_js(val);
}


#define UNPAIRED_OPEN {
#define UNPAIRED_CLOSE }
#define JSFILE(junk, rest...) \
    EM_JS_MACROS(void, js2python_init_js, (void), UNPAIRED_OPEN rest)

#include "jsmemops.h"
#include "js2python.js"
__attribute__((constructor)) void js2python_init(void) {
  js2python_init_js();
}
