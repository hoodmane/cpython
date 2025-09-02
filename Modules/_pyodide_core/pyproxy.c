#include "Python.h"
#include "python2js.h"
#include "js2python.h"
#include "emscripten.h"
#include "error_handling.h"

#define HAS_CONTAINS           (1 << 0)
#define HAS_GET                (1 << 1)
#define HAS_LENGTH             (1 << 2)
#define HAS_SET                (1 << 3)
#define IS_CALLABLE            (1 << 4)

EM_JS_VAL(JsVal, pyproxy_new, (PyObject * ptrobj), {
  return Module.pyproxy_new(ptrobj);
});

EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_str(PyObject* pyobj)
{
  PyObject* pystr = NULL;
  JsVal jsrepr = JS_ERROR;

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

static int
type_getflags(PyTypeObject* obj_type)
{
  PySequenceMethods null_seq_proto = { 0 };
  PySequenceMethods* seq_proto =
    obj_type->tp_as_sequence ? obj_type->tp_as_sequence : &null_seq_proto;

  PyMappingMethods null_map_proto = { 0 };
  PyMappingMethods* map_proto =
    obj_type->tp_as_mapping ? obj_type->tp_as_mapping : &null_map_proto;

#define SET_FLAG_IF(flag, cond)                                                \
  if (cond) {                                                                  \
    result |= flag;                                                            \
  }

  int result = 0;
  SET_FLAG_IF(HAS_CONTAINS, seq_proto->sq_contains);
  if (map_proto->mp_subscript || seq_proto->sq_item) {
    result |= HAS_GET;
  }
  SET_FLAG_IF(HAS_LENGTH, seq_proto->sq_length || map_proto->mp_length);
  SET_FLAG_IF(HAS_SET, map_proto->mp_ass_subscript || seq_proto->sq_ass_item);
  SET_FLAG_IF(IS_CALLABLE, obj_type->tp_call);
  return result;

#undef SET_FLAG_IF
}

EMSCRIPTEN_KEEPALIVE int
pyproxy_getflags(PyObject* pyobj)
{
  PyTypeObject* obj_type = Py_TYPE(pyobj);
  return type_getflags(obj_type);
}

#define Py_ENTER()
#define Py_EXIT()

EMSCRIPTEN_KEEPALIVE int
_pyproxy_contains(PyObject* pyobj, JsVal idkey)
{
  PyObject* pykey = NULL;
  int result = -1;

  pykey = js2python(idkey);
  FAIL_IF_NULL(pykey);
  result = PySequence_Contains(pyobj, pykey);

finally:
  Py_CLEAR(pykey);
  return result;
}


EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_getitem(PyObject* pyobj,
                 JsVal jskey)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyresult = NULL;
  JsVal result;

  pykey = js2python(jskey);
  FAIL_IF_NULL(pykey);
  pyresult = PyObject_GetItem(pyobj, pykey);
  FAIL_IF_NULL(pyresult);
  result = python2js(pyresult);
  FAIL_IF_JS_ERROR(result);

  success = true;
finally:
  if (!success && (PyErr_ExceptionMatches(PyExc_KeyError) ||
                   PyErr_ExceptionMatches(PyExc_IndexError))) {
    PyErr_Clear();
  }
  Py_CLEAR(pykey);
  Py_CLEAR(pyresult);
  if (!success) {
    return JS_ERROR;
  }
  return result;
}

EMSCRIPTEN_KEEPALIVE int
_pyproxy_setitem(PyObject* pyobj, JsVal jskey, JsVal jsval)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyval = NULL;

  pykey = js2python(jskey);
  FAIL_IF_NULL(pykey);
  pyval = js2python(jsval);
  FAIL_IF_NULL(pyval);
  FAIL_IF_MINUS_ONE(PyObject_SetItem(pyobj, pykey, pyval));

  success = true;
finally:
  Py_CLEAR(pykey);
  Py_CLEAR(pyval);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE int
_pyproxy_delitem(PyObject* pyobj, JsVal idkey)
{
  bool success = false;
  PyObject* pykey = NULL;

  pykey = js2python(idkey);
  FAIL_IF_NULL(pykey);
  FAIL_IF_MINUS_ONE(PyObject_DelItem(pyobj, pykey));

  success = true;
finally:
  Py_CLEAR(pykey);
  return success ? 0 : -1;
}


/**
 * This sets up a call to _PyObject_Vectorcall. It's a helper function for
 * callPyObjectKwargs. This is the primary entrypoint from JavaScript into
 * Python code.
 *
 * Vectorcall expects the arguments to be communicated as:
 *
 *  PyObject*const *args: the positional arguments and followed by the keyword
 *    arguments
 *
 *  size_t nargs_with_flag : the number of arguments plus a flag
 *      PY_VECTORCALL_ARGUMENTS_OFFSET. The flag PY_VECTORCALL_ARGUMENTS_OFFSET
 *      indicates that we left an initial entry in the array to be used as a
 *      self argument in case the callee is a bound method.
 *
 *  PyObject* kwnames : a tuple of the keyword argument names. The length of
 *      this tuple tells CPython how many key word arguments there are.
 *
 * Our arguments are:
 *
 *   callable : The object to call.
 *   args : The list of JavaScript arguments, both positional and kwargs.
 *   numposargs : The number of positional arguments.
 *   kwnames : List of names of the keyword arguments
 *   numkwargs : The length of kwargs
 *
 *   Returns: The return value translated to JavaScript.
 */
EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_apply(PyObject* callable,
               JsVal jsargs,
               size_t numposargs,
               JsVal jskwnames,
               size_t numkwargs)
{
  size_t total_args = numposargs + numkwargs;
  size_t last_converted_arg = total_args;
  PyObject* pyargs_array[total_args + 1];
  PyObject** pyargs = pyargs_array;
  pyargs++; // leave a space for self argument in case callable is a bound
            // method
  PyObject* pykwnames = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  // Put both arguments and keyword arguments into pyargs
  for (Py_ssize_t i = 0; i < total_args; ++i) {
    JsVal jsitem = JsvArray_Get(jsargs, i);
    // pyitem is moved into pyargs so we don't need to clear it later.
    PyObject* pyitem = js2python(jsitem);
    if (pyitem == NULL) {
      last_converted_arg = i;
      FAIL();
    }
    pyargs[i] = pyitem; // pyitem is moved into pyargs.
  }
  if (numkwargs > 0) {
    // Put names of keyword arguments into a tuple
    pykwnames = PyTuple_New(numkwargs);
    for (Py_ssize_t i = 0; i < numkwargs; i++) {
      JsVal jsitem = JsvArray_Get(jskwnames, i);
      // pyitem is moved into pykwargs so we don't need to clear it later.
      PyObject* pyitem = js2python(jsitem);
      PyTuple_SET_ITEM(pykwnames, i, pyitem);
    }
  }
  // Tell callee that we left space for a self argument
  size_t nargs_with_flag = numposargs | PY_VECTORCALL_ARGUMENTS_OFFSET;
  pyresult = _PyObject_Vectorcall(callable, pyargs, nargs_with_flag, pykwnames);
  FAIL_IF_NULL(pyresult);
  result = python2js(pyresult);

finally:
  // If we failed to convert one of the arguments, then pyargs is partially
  // uninitialized. Only clear the part that actually has stuff in it.
  for (Py_ssize_t i = 0; i < last_converted_arg; i++) {
    Py_CLEAR(pyargs[i]);
  }
  Py_CLEAR(pyresult);
  Py_CLEAR(pykwnames);
  return result;
}

