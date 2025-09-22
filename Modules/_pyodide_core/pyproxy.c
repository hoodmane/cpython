#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif

#include "Python.h"
#include "jslib.h"
#include "python2js.h"
#include "js2python.h"
#include "emscripten.h"
#include "error_handling.h"
#include "pytypedefs.h"
#include "pyproxy.h"
#include "jsproxy.h"


#include "clinic/pyproxy.c.h"
/*[clinic input]
module _pyodide_core
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=fcac4389838df104]*/

#define Py_ENTER()
#define Py_EXIT()

#define HAS_CONTAINS           (1 << 0)
#define HAS_GET                (1 << 1)
#define HAS_LENGTH             (1 << 2)
#define HAS_SET                (1 << 3)
#define IS_CALLABLE            (1 << 4)
#define IS_DICT                (1 << 5)
#define IS_GENERATOR           (1 << 6)
#define IS_ITERABLE            (1 << 7)
#define IS_ITERATOR            (1 << 8)
#define IS_MUTABLE_SEQUENCE    (1 << 9)
#define IS_SEQUENCE            (1 << 10)


PyAPI_FUNC(int) _PyGen_FetchStopIterationValue(PyObject **);


EM_JS_VAL(JsVal, _PyProxy_New, (PyObject * ptrobj), {
  return Module.pyproxy_new(ptrobj);
});

EM_JS_VAL(JsVal,
_PyProxy_NewEx,
(PyObject * ptrobj, bool capture_this, bool roundtrip, bool gcRegister),
{
  return Module.pyproxy_new(ptrobj, {
    props: { captureThis: !!capture_this, roundtrip: !!roundtrip },
    gcRegister,
  });
});

EM_JS(int, _PyProxy_Check, (JsVal val), {
  return API.isPyProxy(val);
});

EM_JS(PyObject*, _PyProxy_AsPyObject, (JsVal val), {
  if (!API.isPyProxy(val) || !PyProxy_IsAlive(val)) {
    return 0;
  }
  return Module.PyProxy_getPtr(val);
});

EM_JS(void, _PyProxy_Destroy, (JsVal px, Js_Identifier* msg_ptr), {
  const { shared, props } = Module.PyProxy_getAttrsQuiet(px);
  if (!shared.ptr) {
    // already destroyed
    return;
  }
  if (props.roundtrip) {
    // Don't destroy roundtrip proxies!
    return;
  }
  let msg = undefined;
  if (msg_ptr) {
    msg = __PyJsvString_FromId(msg_ptr);
  }
  Module.pyproxy_destroy(px, msg, false);
});

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_str(PyObject* pyobj)
{
  PyObject* pystr = NULL;
  JsVal jsrepr = JS_ERROR;

  pystr = PyObject_Str(pyobj);
  FAIL_IF_NULL(pystr);
  jsrepr = _Py_python2js(pystr);

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
_PyProxy_type(PyObject* ptrobj)
{
  return _PyJsvUTF8ToString(Py_TYPE(ptrobj)->tp_name);
}

static PyObject* Generator;
static PyObject* Sequence;
static PyObject* MutableSequence;

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
  SET_FLAG_IF(IS_DICT, Py_Is(obj_type, &PyDict_Type));
  int isgen = PyObject_IsSubclass((PyObject*)obj_type, Generator);
  FAIL_IF_MINUS_ONE(isgen);
  SET_FLAG_IF(IS_GENERATOR, isgen);
  SET_FLAG_IF(IS_ITERABLE, obj_type->tp_iter || seq_proto->sq_item);

  extern PyObject* _PyObject_NextNotImplemented(PyObject *);
  if (obj_type->tp_iternext != NULL &&
      obj_type->tp_iternext != &_PyObject_NextNotImplemented) {
    result &= ~IS_ITERABLE;
    result |= IS_ITERATOR;
  }
  // A sequence has __len__, __getitem__, __contains__, and __iter__ so if any
  // of these settings is missing, can skip the IsInstance check.
  if (((~result) & (HAS_LENGTH | HAS_GET | HAS_CONTAINS | IS_ITERABLE)) == 0) {
    int is_sequence = PyObject_IsSubclass((PyObject*)obj_type, Sequence);
    FAIL_IF_MINUS_ONE(is_sequence);
    int is_mutable_sequence =
      is_sequence ? PyObject_IsSubclass((PyObject*)obj_type, MutableSequence)
                  : 0;
    FAIL_IF_MINUS_ONE(is_mutable_sequence);
    SET_FLAG_IF(IS_SEQUENCE, is_sequence);
    SET_FLAG_IF(IS_MUTABLE_SEQUENCE, is_mutable_sequence);
  }

  return result;
finally:
  return -1;

#undef SET_FLAG_IF
}

static int dict_flags;
static int tuple_flags;
static int list_flags;

EMSCRIPTEN_KEEPALIVE int
_PyProxy_getflags(PyObject* pyobj)
{
  // Fast paths for some common cases
  if (PyDict_CheckExact(pyobj)) {
    int result = dict_flags;
    return result;
  }
  if (PyTuple_CheckExact(pyobj)) {
    int result = tuple_flags;
    return result;
  }
  if (PyList_CheckExact(pyobj)) {
    int result = list_flags;
    return result;
  }
  PyTypeObject* obj_type = Py_TYPE(pyobj);
  return type_getflags(obj_type);
}

EMSCRIPTEN_KEEPALIVE int
_PyProxy_hasattr(PyObject* pyobj, JsVal jskey)
{
  PyObject* pykey = NULL;
  int result = -1;

  pykey = _Py_js2python(jskey);
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

EM_JS(JsVal, _PyProxy_cache_get, (JsVal proxyCache, PyObject* descr), {
  const proxy = proxyCache.get(descr);
  if (!proxy) {
    return Module.error;
  }
  // Okay found a proxy. Is it alive?
  if (PyProxy_IsAlive(proxy)) {
    return proxy;
  } else {
    // It's dead, tidy up
    proxyCache.delete(descr);
    return Module.error;
  }
})

// clang-format off
EM_JS(void,
_PyProxy_cache_set,
(JsVal proxyCache, PyObject* descr, JsVal proxy), {
  proxyCache.set(descr, proxy);
})
// clang-format on

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_getattr(PyObject* pyobj, JsVal key, JsVal proxyCache)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pydescr = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  pykey = _Py_js2python(key);
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
  JsVal cached_proxy = _PyProxy_cache_get(proxyCache, pydescr); /* borrowed */
  if (!_PyJsvError_Check(cached_proxy)) {
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
  result = _Py_python2js(pyresult);
  if (_PyProxy_Check(result)) {
    // If a getter returns a different object every time, this could potentially
    // fill up the cache with a lot of junk. If this is a problem, the user will
    // have to manually destroy the attributes.
    _PyProxy_cache_set(proxyCache, pydescr, result);
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
_PyProxy_setattr(PyObject* pyobj, JsVal key, JsVal value)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyval = NULL;

  pykey = _Py_js2python(key);
  FAIL_IF_NULL(pykey);
  pyval = _Py_js2python(value);
  FAIL_IF_NULL(pyval);
  FAIL_IF_MINUS_ONE(PyObject_SetAttr(pyobj, pykey, pyval));

  success = true;
finally:
  Py_CLEAR(pykey);
  Py_CLEAR(pyval);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE int
_PyProxy_delattr(PyObject* pyobj, JsVal idkey)
{
  bool success = false;
  PyObject* pykey = NULL;

  pykey = _Py_js2python(idkey);
  FAIL_IF_NULL(pykey);
  FAIL_IF_MINUS_ONE(PyObject_DelAttr(pyobj, pykey));

  success = true;
finally:
  Py_CLEAR(pykey);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_ownKeys(PyObject* pyobj)
{
  bool success = false;
  PyObject* pydir = NULL;

  pydir = PyObject_Dir(pyobj);
  FAIL_IF_NULL(pydir);

  JsVal dir = _PyJsvArray_New();
  Py_ssize_t n = PyList_Size(pydir);
  FAIL_IF_MINUS_ONE(n);
  for (Py_ssize_t i = 0; i < n; ++i) {
    PyObject* pyentry = PyList_GetItem(pydir, i); /* borrowed */
    JsVal entry = _Py_python2js(pyentry);
    FAIL_IF_JS_ERROR(entry);
    _PyJsvArray_Push(dir, entry);
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
_PyProxy_contains(PyObject* pyobj, JsVal idkey)
{
  PyObject* pykey = NULL;
  int result = -1;

  pykey = _Py_js2python(idkey);
  FAIL_IF_NULL(pykey);
  result = PySequence_Contains(pyobj, pykey);

finally:
  Py_CLEAR(pykey);
  return result;
}


EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_getitem(PyObject* pyobj,
                 JsVal jskey)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyresult = NULL;
  JsVal result;

  pykey = _Py_js2python(jskey);
  FAIL_IF_NULL(pykey);
  pyresult = PyObject_GetItem(pyobj, pykey);
  FAIL_IF_NULL(pyresult);
  result = _Py_python2js(pyresult);
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
_PyProxy_setitem(PyObject* pyobj, JsVal jskey, JsVal jsval)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyval = NULL;

  pykey = _Py_js2python(jskey);
  FAIL_IF_NULL(pykey);
  pyval = _Py_js2python(jsval);
  FAIL_IF_NULL(pyval);
  FAIL_IF_MINUS_ONE(PyObject_SetItem(pyobj, pykey, pyval));

  success = true;
finally:
  Py_CLEAR(pykey);
  Py_CLEAR(pyval);
  return success ? 0 : -1;
}

EMSCRIPTEN_KEEPALIVE int
_PyProxy_delitem(PyObject* pyobj, JsVal idkey)
{
  bool success = false;
  PyObject* pykey = NULL;

  pykey = _Py_js2python(idkey);
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
 *  PyObject* const *args: the positional arguments and followed by the keyword
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
_PyProxy_apply(PyObject* callable,
               JsVal jsargs,
               Py_ssize_t numposargs,
               JsVal jskwnames,
               Py_ssize_t numkwargs)
{
  Py_ssize_t total_args = numposargs + numkwargs;
  Py_ssize_t last_converted_arg = total_args;
  PyObject* pyargs_array[total_args + 1];
  PyObject** pyargs = pyargs_array;
  pyargs++; // leave a space for self argument in case callable is a bound
            // method
  PyObject* pykwnames = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  // Put both arguments and keyword arguments into pyargs
  for (Py_ssize_t i = 0; i < total_args; ++i) {
    JsVal jsitem = _PyJsvArray_Get(jsargs, i);
    // pyitem is moved into pyargs so we don't need to clear it later.
    PyObject* pyitem = _Py_js2python(jsitem);
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
      JsVal jsitem = _PyJsvArray_Get(jskwnames, i);
      // pyitem is moved into pykwargs so we don't need to clear it later.
      PyObject* pyitem = _Py_js2python(jsitem);
      PyTuple_SET_ITEM(pykwnames, i, pyitem);
    }
  }
  // Tell callee that we left space for a self argument
  size_t nargs_with_flag = numposargs | PY_VECTORCALL_ARGUMENTS_OFFSET;
  pyresult = _PyObject_Vectorcall(callable, pyargs, nargs_with_flag, pykwnames);
  FAIL_IF_NULL(pyresult);
  result = _Py_python2js(pyresult);

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

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_IterNext(PyObject* iterator)
{
  PyObject* item = PyIter_Next(iterator);
  if (item == NULL) {
    return JS_ERROR;
  }
  JsVal result = _Py_python2js(item);
  Py_CLEAR(item);
  return result;
}

EM_JS(JsVal, _PyProxyGen_make_result, (bool done, JsVal value), {
  return { done : !!done, value };
})

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxyGen_Send(PyObject* receiver, JsVal jsval)
{
  bool success = false;
  PyObject* v = NULL;
  PyObject* retval = NULL;

  v = _Py_js2python(jsval);
  FAIL_IF_NULL(v);
  PySendResult status = PyIter_Send(receiver, v, &retval);
  if (status == PYGEN_ERROR) {
    FAIL();
  }
  JsVal result = _Py_python2js(retval);
  FAIL_IF_JS_ERROR(result);

  success = true;
finally:
  Py_CLEAR(v);
  Py_CLEAR(retval);
  if (!success) {
    return JS_ERROR;
  }
  return _PyProxyGen_make_result(status == PYGEN_RETURN, result);
}


EMSCRIPTEN_KEEPALIVE
JsVal
_PyProxyGen_Return(PyObject* receiver, JsVal jsval)
{
  bool success = false;
  PySendResult status = PYGEN_ERROR;
  PyObject* pyresult = NULL;

  JsVal result;

  // Throw GeneratorExit into generator
  pyresult =
    PyObject_CallMethodOneArg(receiver, &_Py_ID(throw), PyExc_GeneratorExit);
  if (pyresult == NULL) {
    if (PyErr_ExceptionMatches(PyExc_GeneratorExit)) {
      // If GeneratorExit comes back out, return original value.
      PyErr_Clear();
      status = PYGEN_RETURN;
      result = jsval;
      success = true;
      goto finally;
    }
    //
    FAIL_IF_MINUS_ONE(_PyGen_FetchStopIterationValue(&pyresult));
    status = PYGEN_RETURN;
  } else {
    status = PYGEN_NEXT;
  }
  result = _Py_python2js(pyresult);
  FAIL_IF_JS_ERROR(result);
  success = true;
finally:
  Py_CLEAR(pyresult);
  if (!success) {
    return JS_ERROR;
  }
  return _PyProxyGen_make_result(status == PYGEN_RETURN, result);
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxyGen_Throw(PyObject* receiver, JsVal jsval)
{
  bool success = false;
  PyObject* pyvalue = NULL;
  PyObject* pyresult = NULL;
  PySendResult status = PYGEN_ERROR;

  JsVal result;

  pyvalue = _Py_js2python(jsval);
  FAIL_IF_NULL(pyvalue);
  if (!PyExceptionInstance_Check(pyvalue)) {
    /* Not something you can raise.  throw() fails. */
    PyErr_Format(PyExc_TypeError,
                 "exceptions must be classes or instances "
                 "deriving from BaseException, not %s",
                 Py_TYPE(pyvalue)->tp_name);
    FAIL();
  }
  pyresult = PyObject_CallMethodOneArg(receiver, &_Py_ID(throw), pyvalue);
  if (pyresult == NULL) {
    FAIL_IF_MINUS_ONE(_PyGen_FetchStopIterationValue(&pyresult));
    status = PYGEN_RETURN;
  } else {
    status = PYGEN_NEXT;
  }
  result = _Py_python2js(pyresult);
  FAIL_IF_JS_ERROR(result);
  success = true;
finally:
  Py_CLEAR(pyresult);
  Py_CLEAR(pyvalue);
  if (!success) {
    return JS_ERROR;
  }
  return _PyProxyGen_make_result(status == PYGEN_RETURN, result);
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_slice_assign(PyObject* pyobj,
                      Py_ssize_t start,
                      Py_ssize_t stop,
                      JsVal val)
{
  PyObject* pyval = NULL;
  PyObject* pyresult = NULL;
  JsVal jsresult = JS_ERROR;

  pyval = _Py_js2python(val);

  Py_ssize_t len = PySequence_Length(pyobj);
  if (len <= stop) {
    stop = len;
  }
  pyresult = PySequence_GetSlice(pyobj, start, stop);
  FAIL_IF_NULL(pyresult);
  FAIL_IF_MINUS_ONE(PySequence_SetSlice(pyobj, start, stop, pyval));
  JsVal proxies = _PyJsvArray_New();
  jsresult = _Py_python2js_deep(pyresult, 1, proxies, Jsv_null, Jsv_null, Jsv_null);

finally:
  Py_CLEAR(pyresult);
  Py_CLEAR(pyval);
  return jsresult;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_pop(PyObject* pyobj, bool pop_start)
{
  PyObject* idx = NULL;
  PyObject* pyresult = NULL;
  JsVal jsresult = JS_ERROR;
  
  if (pop_start) {
    idx = PyLong_FromLong(0);
    FAIL_IF_NULL(idx);
    pyresult = PyObject_CallMethodOneArg(pyobj, &_Py_ID(pop), idx);
  } else {
    pyresult = PyObject_CallMethodNoArgs(pyobj, &_Py_ID(pop));
  }
  if (pyresult != NULL) {
    jsresult = _Py_python2js(pyresult);
    FAIL_IF_JS_ERROR(jsresult);
  } else {
    if (PyErr_ExceptionMatches(PyExc_IndexError)) {
      PyErr_Clear();
      jsresult = Jsv_undefined;
    } else {
      FAIL();
    }
  }
finally:
  Py_CLEAR(idx);
  Py_CLEAR(pyresult);
  return jsresult;
}

/*[clinic input]
_pyodide_core.create_proxy

    obj: object
        The object to wrap.

    /
    *

    capture_this: bool = False
        If the object is callable, should ``this`` be passed as the first
        argument when calling it from JavaScript.

    roundtrip: bool = True
        When the proxy is converted back from JavaScript to Python, if this is
        True it is converted into a double proxy. If False, it is
        unwrapped into a Python object. In the case that roundtrip is
        True it is possible to unwrap a double proxy with the
        JsDoubleProxy.unwrap method. This is useful to allow easier
        control of lifetimes from Python:


Create a JsProxy of a pyodide.ffi.PyProxy.

This allows explicit control over the lifetime of the PyProxy from Python.
Call JsDoubleProxy.destroy API when done.
[clinic start generated code]*/

static PyObject *
_pyodide_core_create_proxy_impl(PyObject *module, PyObject *obj,
                                int capture_this, int roundtrip)
/*[clinic end generated code: output=365b44a6c783bf4d input=52e14cb92c4ac57d]*/
{
  bool gc_register = true;
  return _PyJsProxy_create(
    _PyProxy_NewEx(obj, capture_this, roundtrip, gc_register));
}

static PyMethodDef methods[] = {
  _PYODIDE_CORE_CREATE_PROXY_METHODDEF
  { NULL } /* Sentinel */
};

int
_Py_pyproxy_init(PyObject* core)
{
  bool success = false;

  PyObject* collections_abc = NULL;

  FAIL_IF_MINUS_ONE(PyModule_AddFunctions(core, methods));
  collections_abc = PyImport_ImportModule("collections.abc");
  FAIL_IF_NULL(collections_abc);
  Generator = PyObject_GetAttrString(collections_abc, "Generator");
  FAIL_IF_NULL(Generator);
  Sequence = PyObject_GetAttrString(collections_abc, "Sequence");
  FAIL_IF_NULL(Sequence);
  MutableSequence = PyObject_GetAttrString(collections_abc, "MutableSequence");
  FAIL_IF_NULL(MutableSequence);

  dict_flags = type_getflags(&PyDict_Type);
  tuple_flags = type_getflags(&PyTuple_Type);
  list_flags = type_getflags(&PyList_Type);

  success = true;
finally:
  Py_CLEAR(collections_abc);
  return success ? 0 : -1;
}
