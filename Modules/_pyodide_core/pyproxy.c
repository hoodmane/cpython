#include "Python.h"
#include "python2js.h"
#include "js2python.h"
#include "emscripten.h"
#include "error_handling.h"

#define Py_ENTER()
#define Py_EXIT()

#define HAS_CONTAINS           (1 << 0)
#define HAS_GET                (1 << 1)
#define HAS_LENGTH             (1 << 2)
#define HAS_SET                (1 << 3)
#define IS_CALLABLE            (1 << 4)
#define IS_ITERABLE            (1 << 7)
#define IS_ITERATOR            (1 << 8)

EM_JS_VAL(JsVal, pyproxy_new, (PyObject * ptrobj), {
  return Module.pyproxy_new(ptrobj);
});

EM_JS(int, PyProxy_Check, (JsVal val), {
  return API.isPyProxy(val);
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
  SET_FLAG_IF(IS_ITERABLE, obj_type->tp_iter || seq_proto->sq_item);

  extern PyObject* _PyObject_NextNotImplemented(PyObject *);
  if (obj_type->tp_iternext != NULL &&
      obj_type->tp_iternext != &_PyObject_NextNotImplemented) {
    result &= ~IS_ITERABLE;
    result |= IS_ITERATOR;
  }

  return result;

#undef SET_FLAG_IF
}

EMSCRIPTEN_KEEPALIVE int
pyproxy_getflags(PyObject* pyobj)
{
  PyTypeObject* obj_type = Py_TYPE(pyobj);
  return type_getflags(obj_type);
}

EMSCRIPTEN_KEEPALIVE int
_pyproxy_hasattr(PyObject* pyobj, JsVal jskey)
{
  PyObject* pykey = NULL;
  int result = -1;

  pykey = js2python(jskey);
  FAIL_IF_NULL(pykey);
  result = PyObject_HasAttr(pyobj, pykey);

finally:
  Py_CLEAR(pykey);
  return result;
}

/* Specialized version of _PyObject_GenericGetAttrWithDict
   specifically for the LOAD_METHOD opcode.

   Return 1 if a method is found, 0 if it's a regular attribute
   from __dict__ or something returned by using a descriptor
   protocol.

   `method` will point to the resolved attribute or NULL.  In the
   latter case, an error will be set.
*/
int
_PyObject_GetMethod(PyObject* obj, PyObject* name, PyObject** method);

EM_JS(JsVal, proxy_cache_get, (JsVal proxyCache, PyObject* descr), {
  const proxy = proxyCache.get(descr);
  if (!proxy) {
    return Module.error;
  }
  // Okay found a proxy. Is it alive?
  if (pyproxyIsAlive(proxy)) {
    return proxy;
  } else {
    // It's dead, tidy up
    proxyCache.delete(descr);
    return Module.error;
  }
})

// clang-format off
EM_JS(void,
proxy_cache_set,
(JsVal proxyCache, PyObject* descr, JsVal proxy), {
  proxyCache.set(descr, proxy);
})
// clang-format on

EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_getattr(PyObject* pyobj, JsVal key, JsVal proxyCache)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pydescr = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  pykey = js2python(key);
  FAIL_IF_NULL(pykey);
  // If it's a method, we use the descriptor pointer as the cache key rather
  // than the actual bound method. This allows us to reuse bound methods from
  // the cache.
  // _PyObject_GetMethod will return true and store a descriptor into pydescr if
  // the attribute we are looking up is a method, otherwise it will return false
  // and set pydescr to the actual attribute (in particular, I believe that it
  // will resolve other types of getter descriptors automatically).
  int is_method = _PyObject_GetMethod(pyobj, pykey, &pydescr);
  FAIL_IF_NULL(pydescr);
  JsVal cached_proxy = proxy_cache_get(proxyCache, pydescr); /* borrowed */
  if (!JsvError_Check(cached_proxy)) {
    result = cached_proxy;
    goto success;
  }
  if (PyErr_Occurred()) {
    FAIL();
  }
  if (is_method) {
    pyresult =
      Py_TYPE(pydescr)->tp_descr_get(pydescr, pyobj, (PyObject*)Py_TYPE(pyobj));
    FAIL_IF_NULL(pyresult);
  } else {
    pyresult = pydescr;
    Py_INCREF(pydescr);
  }
  result = python2js(pyresult);
  if (PyProxy_Check(result)) {
    // If a getter returns a different object every time, this could potentially
    // fill up the cache with a lot of junk. If this is a problem, the user will
    // have to manually destroy the attributes.
    proxy_cache_set(proxyCache, pydescr, result);
  }

success:
  success = true;
finally:
  Py_CLEAR(pykey);
  Py_CLEAR(pydescr);
  Py_CLEAR(pyresult);
  if (!success) {
    if (PyErr_ExceptionMatches(PyExc_AttributeError)) {
      PyErr_Clear();
    }
  }
  return result;
};

EMSCRIPTEN_KEEPALIVE int
_pyproxy_setattr(PyObject* pyobj, JsVal key, JsVal value)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyval = NULL;

  pykey = js2python(key);
  FAIL_IF_NULL(pykey);
  pyval = js2python(value);
  FAIL_IF_NULL(pyval);
  FAIL_IF_MINUS_ONE(PyObject_SetAttr(pyobj, pykey, pyval));

  success = true;
finally:
  Py_CLEAR(pykey);
  Py_CLEAR(pyval);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE int
_pyproxy_delattr(PyObject* pyobj, JsVal idkey)
{
  bool success = false;
  PyObject* pykey = NULL;

  pykey = js2python(idkey);
  FAIL_IF_NULL(pykey);
  FAIL_IF_MINUS_ONE(PyObject_DelAttr(pyobj, pykey));

  success = true;
finally:
  Py_CLEAR(pykey);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE JsVal
_pyproxy_ownKeys(PyObject* pyobj)
{
  bool success = false;
  PyObject* pydir = NULL;

  pydir = PyObject_Dir(pyobj);
  FAIL_IF_NULL(pydir);

  JsVal dir = JsvArray_New();
  Py_ssize_t n = PyList_Size(pydir);
  FAIL_IF_MINUS_ONE(n);
  for (Py_ssize_t i = 0; i < n; ++i) {
    PyObject* pyentry = PyList_GetItem(pydir, i); /* borrowed */
    JsVal entry = python2js(pyentry);
    FAIL_IF_JS_ERROR(entry);
    JsvArray_Push(dir, entry);
  }

  success = true;
finally:
  Py_CLEAR(pydir);
  if (!success) {
    return JS_ERROR;
  }
  return dir;
}

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

EM_JS(JsVal, _pyproxyGen_make_result, (bool done, JsVal value), {
  return { done : !!done, value };
})

EMSCRIPTEN_KEEPALIVE JsVal
_pyproxyGen_Send(PyObject* receiver, JsVal jsval)
{
  bool success = false;
  PyObject* v = NULL;
  PyObject* retval = NULL;

  v = js2python(jsval);
  FAIL_IF_NULL(v);
  PySendResult status = PyIter_Send(receiver, v, &retval);
  if (status == PYGEN_ERROR) {
    FAIL();
  }
  JsVal result = python2js(retval);
  FAIL_IF_JS_ERROR(result);

  success = true;
finally:
  Py_CLEAR(v);
  Py_CLEAR(retval);
  if (!success) {
    return JS_ERROR;
  }
  return _pyproxyGen_make_result(status == PYGEN_RETURN, result);
}
