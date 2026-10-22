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
#include "pycore_runtime.h"       // _Py_ID()


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
#define IS_JS_JSON_DICT        (1 << 11)
#define IS_JS_JSON_SEQUENCE    (1 << 12)
#define IS_AWAITABLE           (1 << 13)
#define IS_BUFFER              (1 << 14)
#define IS_ASYNC_ITERABLE      (1 << 15)
#define IS_ASYNC_ITERATOR      (1 << 16)
#define IS_ASYNC_GENERATOR     (1 << 17)


PyAPI_FUNC(int) _PyGen_FetchStopIterationValue(PyObject **);

static PyObject* asyncio_mod;

/**
 * Lazily import asyncio and return a new reference to it.
 */
static PyObject*
get_asyncio(void)
{
  if (asyncio_mod == NULL) {
    asyncio_mod = PyImport_ImportModule("asyncio");
  }
  return asyncio_mod;
}


EM_JS_VAL(JsVal, _PyProxy_New, (PyObject *ptrobj), {
  return Module.pyproxy_new(ptrobj);
});

EM_JS_VAL(JsVal,
_PyProxy_NewEx,
(PyObject *ptrobj, bool capture_this, bool roundtrip, bool gcRegister, bool jsonAdaptor),
{
  return Module.pyproxy_new(ptrobj, {
    props: { captureThis: !!capture_this, roundtrip: !!roundtrip },
    gcRegister,
    jsonAdaptor,
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

EM_JS(void, _Py_gc_register_pyproxies, (JsVal proxies), {
  for (let px of proxies) {
    Module.gc_register_proxy(API.PyProxy_getAttrs(px).shared);
  }
});

EM_JS(void, _PyProxy_Destroy, (JsVal px, Js_Identifier* msg_ptr), {
  const { shared, props } = API.PyProxy_getAttrsQuiet(px);
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
static PyObject* AsyncGenerator;
static PyObject* Sequence;
static PyObject* MutableSequence;

/**
 * Is the generator object a "legacy" coroutine, i.e., a generator decorated
 * with @types.coroutine? Copied from genobject.c.
 */
static bool
gen_is_coroutine(PyObject* o)
{
  if (PyGen_CheckExact(o)) {
    PyCodeObject* code = PyGen_GetCode((PyGenObject*)o);
    bool result = code->co_flags & CO_ITERABLE_COROUTINE;
    Py_DECREF(code);
    return result;
  }
  return false;
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

  PyAsyncMethods null_async_proto = { 0 };
  PyAsyncMethods* async_proto =
    obj_type->tp_as_async ? obj_type->tp_as_async : &null_async_proto;

  PyBufferProcs null_buffer_proto = { 0 };
  PyBufferProcs* buffer_proto =
    obj_type->tp_as_buffer ? obj_type->tp_as_buffer : &null_buffer_proto;

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
  int isasyncgen = PyObject_IsSubclass((PyObject*)obj_type, AsyncGenerator);
  FAIL_IF_MINUS_ONE(isasyncgen);
  SET_FLAG_IF(IS_ASYNC_GENERATOR, isasyncgen);
  SET_FLAG_IF(IS_ITERABLE, obj_type->tp_iter || seq_proto->sq_item);
  SET_FLAG_IF(IS_ASYNC_ITERABLE, async_proto->am_aiter);

  extern PyObject* _PyObject_NextNotImplemented(PyObject *);
  if (obj_type->tp_iternext != NULL &&
      obj_type->tp_iternext != &_PyObject_NextNotImplemented) {
    result &= ~IS_ITERABLE;
    result |= IS_ITERATOR;
  }
  if (async_proto->am_anext) {
    result &= ~IS_ASYNC_ITERABLE;
    result |= IS_ASYNC_ITERATOR;
  }
  // GET_AWAITABLE in ceval accepts coroutines, legacy generator-based
  // coroutines, and objects with an __await__ method. Legacy coroutines can't
  // be detected from the type alone, see _PyProxy_getflags.
  SET_FLAG_IF(IS_AWAITABLE,
              Py_Is(obj_type, &PyCoro_Type) || async_proto->am_await);
  SET_FLAG_IF(IS_BUFFER, buffer_proto->bf_getbuffer);
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
_PyProxy_getflags(PyObject* pyobj, bool is_js_json)
{
  // Fast paths for some common cases
  if (PyDict_CheckExact(pyobj)) {
    int result = dict_flags;
    if (is_js_json) {
      result |= IS_JS_JSON_DICT;
    }
    return result;
  }
  if (PyTuple_CheckExact(pyobj)) {
    int result = tuple_flags;
    if (is_js_json) {
      result |= IS_JS_JSON_SEQUENCE;
    }
    return result;
  }
  if (PyList_CheckExact(pyobj)) {
    int result = list_flags;
    if (is_js_json) {
      result |= IS_JS_JSON_SEQUENCE;
    }
    return result;
  }
  PyTypeObject* obj_type = Py_TYPE(pyobj);
  int result = type_getflags(obj_type);
  if (result == -1) {
    return -1;
  }
  // If the object is a legacy generator-based coroutine we can't tell just by
  // looking at the type, we have to check the code flags of the generator.
  if (!(result & IS_AWAITABLE) && (result & IS_GENERATOR) &&
      gen_is_coroutine(pyobj)) {
    result |= IS_AWAITABLE;
  }
  if (is_js_json) {
    if (result & IS_SEQUENCE) {
      result |= IS_JS_JSON_SEQUENCE;
    } else if (result & HAS_GET) {
      result |= IS_JS_JSON_DICT;
    }
  }
  return result;
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

/**
 * Used by pyproxy_iter_next and pyproxy_get_item for handling json adaptors.
 *
 * If is_json_adaptor,
 *  1. check json adaptor cache for x, if it's already there get existing value
 *  2. If it's not already there, convert x. Add an appropriate json adaptor
 *     type flag if x needs it.
 *  3. Add result to proxy cache.
 */
static JsVal
python2js_json_adaptor(PyObject* x, JsVal proxyCache, bool is_json_adaptor)
{
  if (!is_json_adaptor) {
    return _Py_python2js(x);
  }
  JsVal cached_proxy = _PyProxy_cache_get(proxyCache, x); /* borrowed */
  if (!_PyJsvError_Check(cached_proxy)) {
    return cached_proxy;
  }
  JsVal result = _Py_python2js_options(x, JS_ERROR, (struct _python2js_options) { .track_proxies=false, .gc_register=true, .is_json_adaptor=is_json_adaptor});
  if (_PyProxy_Check(result)) {
    _PyProxy_cache_set(proxyCache, x, result);
  }
  return result;
}

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
_PyProxy_GetItem(PyObject* pyobj,
                 JsVal jskey,
                 JsVal proxyCache,
                 bool is_json_adaptor)
{
  bool success = false;
  PyObject* pykey = NULL;
  PyObject* pyresult = NULL;
  JsVal result;

  pykey = _Py_js2python(jskey);
  FAIL_IF_NULL(pykey);
  pyresult = PyObject_GetItem(pyobj, pykey);
  FAIL_IF_NULL(pyresult);
  result = python2js_json_adaptor(pyresult, proxyCache, is_json_adaptor);
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
_PyProxy_SetItem(PyObject* pyobj, JsVal jskey, JsVal jsval)
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
_PyProxy_DelItem(PyObject* pyobj, JsVal idkey)
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
_PyProxy_IterNext(PyObject* iterator, JsVal proxyCache, bool is_json_adaptor)
{
  PyObject* item = PyIter_Next(iterator);
  if (item == NULL) {
    return JS_ERROR;
  }
  JsVal result = python2js_json_adaptor(item, proxyCache, is_json_adaptor);
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

///////////////////////////////////////////////////////////////////////////////
//
// Async iteration

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxyGen_asend(PyObject* receiver, JsVal jsval)
{
  PyObject* v = NULL;
  PyObject* asend = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  v = _Py_js2python(jsval);
  FAIL_IF_NULL(v);
  if (PyObject_GetOptionalAttr(receiver, &_Py_ID(asend), &asend) == -1) {
    FAIL();
  }
  if (asend != NULL) {
    pyresult = PyObject_CallOneArg(asend, v);
  } else {
    PyTypeObject* t = Py_TYPE(receiver);
    if (t->tp_as_async == NULL || t->tp_as_async->am_anext == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "'%.200s' object is not an async iterator",
                   t->tp_name);
      FAIL();
    }
    pyresult = (*t->tp_as_async->am_anext)(receiver);
  }
  FAIL_IF_NULL(pyresult);
  result = _Py_python2js(pyresult);

finally:
  Py_CLEAR(v);
  Py_CLEAR(asend);
  Py_CLEAR(pyresult);
  return result;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxyGen_areturn(PyObject* receiver)
{
  PyObject* pyresult =
    PyObject_CallMethodOneArg(receiver, &_Py_ID(athrow), PyExc_GeneratorExit);
  if (pyresult == NULL) {
    return JS_ERROR;
  }
  JsVal result = _Py_python2js(pyresult);
  Py_DECREF(pyresult);
  return result;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxyGen_athrow(PyObject* receiver, JsVal jsval)
{
  PyObject* v = NULL;
  PyObject* pyresult = NULL;
  JsVal result = JS_ERROR;

  v = _Py_js2python(jsval);
  FAIL_IF_NULL(v);
  if (!PyExceptionInstance_Check(v)) {
    /* Not something you can raise.  throw() fails. */
    PyErr_Format(PyExc_TypeError,
                 "exceptions must be classes or instances "
                 "deriving from BaseException, not %s",
                 Py_TYPE(v)->tp_name);
    FAIL();
  }
  pyresult = PyObject_CallMethodOneArg(receiver, &_Py_ID(athrow), v);
  FAIL_IF_NULL(pyresult);
  result = _Py_python2js(pyresult);

finally:
  Py_CLEAR(v);
  Py_CLEAR(pyresult);
  return result;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_aiter_next(PyObject* aiterator)
{
  PyTypeObject* t = Py_TYPE(aiterator);
  if (t->tp_as_async == NULL || t->tp_as_async->am_anext == NULL) {
    PyErr_Format(
      PyExc_TypeError, "'%.200s' object is not an async iterator", t->tp_name);
    return JS_ERROR;
  }
  PyObject* awaitable = (*t->tp_as_async->am_anext)(aiterator);
  if (awaitable == NULL) {
    return JS_ERROR;
  }
  JsVal result = _Py_python2js(awaitable);
  Py_DECREF(awaitable);
  return result;
}

/**
 * Is f a coroutine function? Used to decide whether to automatically schedule
 * the coroutine returned from a call from JavaScript into Python.
 */
EMSCRIPTEN_KEEPALIVE bool
_PyProxy_iscoroutinefunction(PyObject* f)
{
  // Some fast paths for common cases to avoid calling into Python
  if (PyMethod_Check(f)) {
    f = PyMethod_GET_FUNCTION(f);
  }
  if (PyFunction_Check(f)) {
    int has_marker = PyObject_HasAttrWithError(f, &_Py_ID(_is_coroutine_marker));
    if (has_marker == -1) {
      PyErr_Clear();
      return false;
    }
    if (!has_marker) {
      PyCodeObject* code = (PyCodeObject*)PyFunction_GET_CODE(f);
      return (code->co_flags) & CO_COROUTINE;
    }
  }

  // Wasn't a basic callable, call into inspect.iscoroutinefunction
  PyObject* inspect = PyImport_ImportModule("inspect");
  if (inspect == NULL) {
    PyErr_Clear();
    return false;
  }
  PyObject* result =
    PyObject_CallMethodOneArg(inspect, &_Py_ID(iscoroutinefunction), f);
  Py_DECREF(inspect);
  if (result == NULL) {
    PyErr_Clear();
    return false;
  }
  bool is_coroutine = Py_IsTrue(result);
  Py_DECREF(result);
  return is_coroutine;
}

///////////////////////////////////////////////////////////////////////////////
//
// Await / "then" Implementation
//
// We want convert the object to a future with `ensure_future` and then make a
// promise that resolves when the future does. We can add a callback to the
// future with future.add_done_callback but we need to make a little python
// closure "FutureDoneCallback" that remembers how to resolve the promise.
//
// From JavaScript we will use the single function _PyProxy_ensure_future, the
// rest of this segment is helper functions for _PyProxy_ensure_future. The
// FutureDoneCallback object is never exposed to the user.

/**
 * A simple Callable python object. Intended to be called with a single argument
 * which is the future that was resolved.
 */
typedef struct
{
  PyObject_HEAD
  /** Will call this function with the result if the future succeeded */
  JsRef resolve_handle;
  /** Will call this function with the error if the future failed */
  JsRef reject_handle;
} FutureDoneCallback;

static void
FutureDoneCallback_dealloc(PyObject* o)
{
  FutureDoneCallback* self = (FutureDoneCallback*)o;
  hiwire_CLEAR(self->resolve_handle);
  hiwire_CLEAR(self->reject_handle);
  Py_TYPE(self)->tp_free((PyObject*)self);
}

EM_JS(void, _Py_call_one_arg_js, (JsVal func, JsVal arg), {
  func(arg);
});

/**
 * Helper method: if the future resolved successfully, call resolve_handle on
 * the result.
 */
static int
FutureDoneCallback_call_resolve(FutureDoneCallback* self, PyObject* result)
{
  JsVal result_js = _Py_python2js(result);
  if (_PyJsvError_Check(result_js)) {
    return -1;
  }
  _Py_call_one_arg_js(hiwire_get(self->resolve_handle), result_js);
  return 0;
}

/**
 * Helper method: if the future threw an error, call reject_handle on a
 * converted exception. The caller leaves the python error indicator set.
 */
static int
FutureDoneCallback_call_reject(FutureDoneCallback* self)
{
  // _Py_pythonexc2js looks up the current exception and wraps it in a Js
  // error.
  JsVal excval = _Py_pythonexc2js();
  if (_PyJsvError_Check(excval)) {
    return -1;
  }
  _Py_call_one_arg_js(hiwire_get(self->reject_handle), excval);
  return 0;
}

/**
 * Intended to be called with a single argument which is the future that was
 * resolved. Resolves the promise as appropriate based on the result of the
 * future.
 */
static PyObject*
FutureDoneCallback_call(PyObject* o, PyObject* args, PyObject* kwargs)
{
  FutureDoneCallback* self = (FutureDoneCallback*)o;
  PyObject* fut;
  if (!PyArg_UnpackTuple(args, "future_done_callback", 1, 1, &fut)) {
    return NULL;
  }
  PyObject* result = PyObject_CallMethodNoArgs(fut, &_Py_ID(result));
  int err;
  if (result != NULL) {
    err = FutureDoneCallback_call_resolve(self, result);
    Py_DECREF(result);
  } else {
    err = FutureDoneCallback_call_reject(self);
  }
  if (err == 0) {
    Py_RETURN_NONE;
  } else {
    return NULL;
  }
}

static PyTypeObject FutureDoneCallbackType = {
  .tp_name = "pyodide.ffi._FutureDoneCallback",
  .tp_doc =
    "Callback for internal use to allow awaiting a future from JavaScript",
  .tp_basicsize = sizeof(FutureDoneCallback),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_dealloc = FutureDoneCallback_dealloc,
  .tp_call = FutureDoneCallback_call,
};

static PyObject*
FutureDoneCallback_cnew(JsVal resolve_handle, JsVal reject_handle)
{
  FutureDoneCallback* self =
    (FutureDoneCallback*)FutureDoneCallbackType.tp_alloc(
      &FutureDoneCallbackType, 0);
  if (self == NULL) {
    return NULL;
  }
  self->resolve_handle = hiwire_new(resolve_handle);
  self->reject_handle = hiwire_new(reject_handle);
  return (PyObject*)self;
}

/**
 * Schedule the awaitable pyobject with asyncio.ensure_future and arrange for
 * resolve_handle or reject_handle to be called when it is done.
 *
 * :param pyobject: An awaitable python object
 * :param resolve_handle: The resolve javascript method for a promise
 * :param reject_handle: The reject javascript method for a promise
 * :return: 0 on success, -1 on failure
 */
EMSCRIPTEN_KEEPALIVE int
_PyProxy_ensure_future(PyObject* pyobject,
                       JsVal resolve_handle,
                       JsVal reject_handle)
{
  bool success = false;
  PyObject* future = NULL;
  PyObject* callback = NULL;
  PyObject* retval = NULL;

  PyObject* asyncio = get_asyncio();
  FAIL_IF_NULL(asyncio);
  future = PyObject_CallMethodOneArg(asyncio, &_Py_ID(ensure_future), pyobject);
  FAIL_IF_NULL(future);
  callback = FutureDoneCallback_cnew(resolve_handle, reject_handle);
  FAIL_IF_NULL(callback);
  retval =
    PyObject_CallMethodOneArg(future, &_Py_ID(add_done_callback), callback);
  FAIL_IF_NULL(retval);

  success = true;
finally:
  Py_CLEAR(future);
  Py_CLEAR(callback);
  Py_CLEAR(retval);
  return success ? 0 : -1;
}

///////////////////////////////////////////////////////////////////////////////
//
// Buffers

EM_JS(JsVal, _Py_array_to_js, (Py_ssize_t * array, int len), {
  return Array.from(HEAP32.subarray(array / 4, array / 4 + len));
})

// clang-format off
EM_JS_VAL(JsVal,
_PyProxy_get_buffer_result,
(void* start_ptr,
 void* smallest_ptr,
 void* largest_ptr,
 int readonly,
 char* format,
 int itemsize,
 JsVal shape,
 JsVal strides,
 Py_buffer* view,
 int c_contiguous,
 int f_contiguous),
{
  format = UTF8ToString(format);
  return {
    start_ptr,
    smallest_ptr,
    largest_ptr,
    readonly : !!readonly,
    format,
    itemsize,
    shape,
    strides,
    view,
    c_contiguous : !!c_contiguous,
    f_contiguous : !!f_contiguous,
  };
})
// clang-format on

/**
 * Get a buffer from the Python object and return a JavaScript object
 * describing it. The Py_buffer is heap allocated and must be released and
 * freed by the caller (PyBufferView.release() does this).
 */
EMSCRIPTEN_KEEPALIVE JsVal
_PyProxy_get_buffer(PyObject* ptrobj)
{
  Py_buffer v;
  // PyBUF_RECORDS_RO requires that suboffsets be NULL but otherwise is the most
  // permissive possible request.
  if (PyObject_GetBuffer(ptrobj, &v, PyBUF_RECORDS_RO) == -1) {
    return JS_ERROR;
  }

  char* start_ptr = v.buf;
  char* smallest_ptr = v.buf;
  char* largest_ptr = v.buf;
  JsVal shape;
  JsVal strides;
  bool c_contiguous;
  bool f_contiguous;

  Py_buffer* view = (Py_buffer*)PyMem_Malloc(sizeof(Py_buffer));
  if (view == NULL) {
    PyBuffer_Release(&v);
    PyErr_NoMemory();
    return JS_ERROR;
  }
  *view = v;

  if (v.ndim == 0) {
    largest_ptr += v.itemsize;
    shape = _PyJsvArray_New();
    strides = _PyJsvArray_New();
    c_contiguous = true;
    f_contiguous = true;
  } else if (v.strides == NULL) {
    // In this case we are a C contiguous buffer
    largest_ptr += v.len;
    Py_ssize_t cstrides[v.ndim];
    PyBuffer_FillContiguousStrides(v.ndim, v.shape, cstrides, v.itemsize, 'C');
    c_contiguous = true;
    f_contiguous = (v.ndim == 1);
    shape = _Py_array_to_js(v.shape, v.ndim);
    strides = _Py_array_to_js(cstrides, v.ndim);
  } else {
    if (v.len != 0) {
      for (int i = 0; i < v.ndim; i++) {
        if (v.strides[i] > 0) {
          largest_ptr += v.strides[i] * (v.shape[i] - 1);
        } else {
          smallest_ptr += v.strides[i] * (v.shape[i] - 1);
        }
      }
      largest_ptr += v.itemsize;
    }
    shape = _Py_array_to_js(v.shape, v.ndim);
    strides = _Py_array_to_js(v.strides, v.ndim);
    c_contiguous = PyBuffer_IsContiguous(&v, 'C');
    f_contiguous = PyBuffer_IsContiguous(&v, 'F');
  }
  return _PyProxy_get_buffer_result(start_ptr,
                                    smallest_ptr,
                                    largest_ptr,
                                    v.readonly,
                                    v.format,
                                    v.itemsize,
                                    shape,
                                    strides,
                                    view,
                                    c_contiguous,
                                    f_contiguous);
}

EMSCRIPTEN_KEEPALIVE void
_PyProxy_release_buffer(Py_buffer* view)
{
  PyBuffer_Release(view);
  PyMem_Free(view);
}

///////////////////////////////////////////////////////////////////////////////
//
// Promise handles and once-callables

/**
 * Wrap a Python callable in a JavaScript function that may be called exactly
 * once. The reference to the Python callable is released when the function is
 * called (or when the wrapper is garbage collected). Useful for the "finally"
 * wrapper on a JsProxy of a promise, and also exposed in the pyodide Python
 * module.
 */
EM_JS_VAL(JsVal, _PyProxy_create_once_callable, (PyObject * obj), {
  _Py_IncRef(obj);
  let alreadyCalled = false;
  function wrapper(... args)
  {
    if (alreadyCalled) {
      throw new Error("OnceProxy can only be called once");
    }
    try {
      return Module.callPyObject(obj, args);
    } finally {
      wrapper.destroy();
    }
  }
  wrapper.destroy = function()
  {
    if (alreadyCalled) {
      throw new Error("OnceProxy has already been destroyed");
    }
    alreadyCalled = true;
    Module.finalizationRegistry.unregister(wrapper);
    _Py_DecRef(obj);
  };
  Module.finalizationRegistry.register(wrapper, { ptr : obj }, wrapper);
  return wrapper;
});

/**
 * Convert the JavaScript result of a promise to Python and call the Python
 * handle_result callback with it. Called from create_promise_handles.
 */
EMSCRIPTEN_KEEPALIVE int
_PyProxy_promise_handles_result_helper(PyObject* handle_result, JsVal jsval)
{
  bool success = false;
  PyObject* pyval = NULL;
  PyObject* result = NULL;

  pyval = _Py_js2python(jsval);
  FAIL_IF_NULL(pyval);
  result = PyObject_CallOneArg(handle_result, pyval);
  FAIL_IF_NULL(result);

  success = true;
finally:
  Py_CLEAR(pyval);
  Py_CLEAR(result);
  return success ? 0 : -1;
}

/**
 * Arguments:
 *  handle_result -- Python callable expecting one argument, called with the
 *  result if the promise is resolved. Can be NULL.
 *
 *  handle_exception -- Python callable expecting one argument, called with the
 *  exception if the promise is rejected. Can be NULL.
 *
 *  done_callback -- A JavaScript callback to be called when the promise is
 *  either resolved or rejected. Can be null.
 *
 * Returns: a pair [onResolved, onRejected].
 *
 * The purpose of this function is to handle memory management when attaching
 * Python functions to Promises. This function stores a reference to both
 * handle_result and handle_exception, and frees both when either onResolved or
 * onRejected is called. Of course if the Promise never resolves then the
 * handles will be leaked. We can't use create_once_callable because either
 * onResolved or onRejected is called but not both. In either case, we release
 * both functions.
 *
 * The return values are intended to be attached to a promise e.g.,
 * some_promise.then(onResolved, onRejected).
 */
// clang-format off
EM_JS_VAL(JsVal, _PyProxy_create_promise_handles, (
  PyObject* handle_result,
  PyObject* handle_exception,
  JsVal done_callback
), {
  if (handle_result) {
    _Py_IncRef(handle_result);
  }
  if (handle_exception) {
    _Py_IncRef(handle_exception);
  }
  if (!done_callback) {
    done_callback = (x) => {};
  }
  let used = false;
  function checkUsed(){
    if (used) {
      throw new Error("One of the promise handles has already been called.");
    }
  }
  function destroy(){
    checkUsed();
    used = true;
    if (handle_result) {
      _Py_DecRef(handle_result);
    }
    if (handle_exception) {
      _Py_DecRef(handle_exception);
    }
  }
  function onFulfilled(res) {
    if (API.finalized) {
      return;
    }
    checkUsed();
    try {
      if (handle_result) {
        if (__PyProxy_promise_handles_result_helper(handle_result, res) === -1) {
          throw __Py_pythonexc2js();
        }
      } else {
        // No handler: pass the value through like Promise.then does.
        return res;
      }
    } finally {
      done_callback(res);
      destroy();
    }
  }
  function onRejected(err) {
    if (API.finalized) {
      return;
    }
    checkUsed();
    try {
      if (handle_exception) {
        return Module.callPyObject(handle_exception, [err]);
      }
      // No handler: propagate the rejection like Promise.then does.
      throw err;
    } finally {
      done_callback(undefined);
      destroy();
    }
  }
  onFulfilled.destroy = destroy;
  onRejected.destroy = destroy;
  return [onFulfilled, onRejected];
})
// clang-format on

/*[clinic input]
_pyodide_core.create_once_callable

    obj: object
    /

Wrap a Python callable in a JavaScript function callable only once.

After being called the proxy will decrement the reference count of the
Callable. The JavaScript function also has a destroy API that can be
used to release the proxy without calling it.
[clinic start generated code]*/

static PyObject *
_pyodide_core_create_once_callable(PyObject *module, PyObject *obj)
/*[clinic end generated code: output=3fe062564a700f55 input=9dce58f8016cea70]*/
{
  JsVal v = _PyProxy_create_once_callable(obj);
  if (_PyJsvError_Check(v)) {
    return NULL;
  }
  return _PyJsProxy_create(v);
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
    _PyProxy_NewEx(obj, capture_this, roundtrip, gc_register, false));
}

static PyMethodDef methods[] = {
  _PYODIDE_CORE_CREATE_PROXY_METHODDEF
  _PYODIDE_CORE_CREATE_ONCE_CALLABLE_METHODDEF
  { NULL } /* Sentinel */
};

int
_Py_pyproxy_init(PyObject* core)
{
  bool success = false;

  PyObject* collections_abc = NULL;

  FAIL_IF_MINUS_ONE(PyModule_AddFunctions(core, methods));
  FAIL_IF_MINUS_ONE(PyType_Ready(&FutureDoneCallbackType));
  collections_abc = PyImport_ImportModule("collections.abc");
  FAIL_IF_NULL(collections_abc);
  Generator = PyObject_GetAttrString(collections_abc, "Generator");
  FAIL_IF_NULL(Generator);
  AsyncGenerator = PyObject_GetAttrString(collections_abc, "AsyncGenerator");
  FAIL_IF_NULL(AsyncGenerator);
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
