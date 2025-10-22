#ifndef Py_BUILD_CORE_BUILTIN
#  define Py_BUILD_CORE_MODULE 1
#endif


#include "jsproxy.h"
#include "pyproxy.h"
#include "jsproxy_call.h"
#include "jslib.h"
#include "error_handling.h"
#include "js2python.h"
#include "python2js.h"
#include "jsmemops.h"

#include "pyidentifier.h"
#include "pycore_genobject.h"
#include "pycore_modsupport.h"
#include "pycore_setobject.h"     // _PySet_Update()
#include "pycore_runtime.h"     // _Py_ID()

#define HAS_GET             (1 << 0)
#define HAS_HAS             (1 << 1)
#define HAS_INCLUDES        (1 << 2)
#define HAS_LENGTH          (1 << 3)
#define HAS_SET             (1 << 4)
#define IS_ARRAY            (1 << 5)
#define IS_CALLABLE         (1 << 6)
#define IS_ERROR            (1 << 7)
#define IS_GENERATOR        (1 << 8)
#define IS_ITERABLE         (1 << 9)
#define IS_ITERATOR         (1 << 10)
#define IS_DOUBLE_PROXY     (1 << 11)
#define IS_PY_JSON_DICT     (1 << 12)
#define IS_PY_JSON_SEQUENCE (1 << 13)

Js_IDENTIFIER(next);


struct CallableFields
{
  JsRef this_;
  vectorcallfunc vectorcall;
};

struct ExceptionFields
{
  PyObject* args;
  PyObject* notes;
  PyObject* traceback;
  PyObject* context;
  PyObject* cause;
  char suppress_context;
};


typedef struct
{
  PyObject_HEAD
  PyObject* dict;
  union {
    struct CallableFields mf;
    struct ExceptionFields ef;
  } tf;
  JsRef js;
} JsProxy;

/*[clinic input]
module _pyodide_core

class _pyodide_core.JsProxy "PyObject *" "JsProxyType"

class _pyodide_core.JsArray "PyObject *" "JsProxyType"
class _pyodide_core.JsDoubleProxy "PyObject *" "JsProxyType"
class _pyodide_core.JsException "PyObject *" "JsProxyType"
class _pyodide_core.JsGenerator "PyObject *" "JsProxyType"
class _pyodide_core.JsMap "PyObject *" "JsProxyType"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=3c5b38b8249a30cb]*/
#include "clinic/jsproxy.c.h"

// Layout of dict and ExceptionFields needs to exactly match the layout of the
// same-name fields of BaseException. Otherwise bad things will happen. Check it
// with static asserts!
_Static_assert(offsetof(PyBaseExceptionObject, dict) == offsetof(JsProxy, dict),
               "dict layout conflict between JsProxy and PyExc_BaseException");

#define CHECK_EXC_FIELD(field)                                                 \
  _Static_assert(                                                              \
    offsetof(PyBaseExceptionObject, field) ==                                  \
      offsetof(JsProxy, tf) + offsetof(struct ExceptionFields, field),         \
    "'" #field "' layout conflict between JsProxy and PyExc_BaseException");

CHECK_EXC_FIELD(args);
CHECK_EXC_FIELD(notes);
CHECK_EXC_FIELD(traceback);
CHECK_EXC_FIELD(context);
CHECK_EXC_FIELD(cause);
CHECK_EXC_FIELD(suppress_context);

#undef CHEC_EXC_FIELD

#define FIELD_SIZE(type, field) sizeof(((type*)0)->field)

_Static_assert(sizeof(PyBaseExceptionObject) ==
                 sizeof(PyObject) + FIELD_SIZE(JsProxy, dict) +
                   sizeof(struct ExceptionFields),
               "size conflict between JsProxy and PyExc_BaseException");
#undef FIELD_SIZE


#define JsProxy_REF(x) ((JsProxy*)x)->js
#define JsProxy_VAL(x) hiwire_get(JsProxy_REF(x))
#define JsProxy_DICT(x) (((JsProxy*)x)->dict)

#define JsMethod_THIS_REF(x) ((JsProxy*)x)->tf.mf.this_
#define JsMethod_THIS(x) _PyJsRef_toVal(JsMethod_THIS_REF(x))
#define JsMethod_VECTORCALL(x) (((JsProxy*)x)->tf.mf.vectorcall)

#define JsException_ARGS(x) (((JsProxy*)x)->tf.ef.args)

static PyObject* collections_abc;
static PyObject* MutableMapping;
static PyObject* JsProxy_metaclass;


static PyObject*
JsProxy_create_pyjson(JsVal object, bool pyjson);

static PyTypeObject*
JsProxy_get_subtype(int flags);

static PyObject*
JsProxy_create_with_type(int type_flags,
                            JsVal object,
                            JsVal this);

static int
JsProxy_getflags(PyObject* self)
{
  PyObject* pyflags =
    PyObject_GetAttr((PyObject*)Py_TYPE(self), &_Py_ID(_js_type_flags));
  if (pyflags == NULL) {
    return -1;
  }
  int result = PyLong_AsLong(pyflags);
  Py_CLEAR(pyflags);
  return result;
}

static int
JsProxy_is_py_json(PyObject* self)
{
  return !!(JsProxy_getflags(self) & (IS_PY_JSON_DICT | IS_PY_JSON_SEQUENCE));
}

static PyObject*
js2python_pyjson(JsVal jsval, bool pyjson)
{
  PyObject* result = NULL;

  result = _Py_js2python_immutable(jsval);
  if (result != NULL) {
    return result;
  }
  return JsProxy_create_pyjson(jsval, pyjson);
}

#define INCLUDE_PYJSON_METHODS(flags)                                          \
  !((flags) & (IS_DOUBLE_PROXY | IS_ITERATOR | IS_CALLABLE | IS_ERROR))


static int
JsProxy_clear(PyObject* self)
{
  int flags = JsProxy_getflags(self);
  if (flags == -1) {
    return -1;
  }
  if ((flags & IS_CALLABLE) && (JsMethod_THIS_REF(self) != NULL)) {
    hiwire_pop(JsMethod_THIS_REF(self));
  }
  if (flags & IS_ERROR) {
    if (((PyTypeObject*)PyExc_Exception)->tp_clear(self)) {
      return -1;
    }
  }
  Py_CLEAR(JsProxy_DICT(self));
  hiwire_CLEAR(JsProxy_REF(self));
  return 0;
}

static void
JsProxy_dealloc(PyObject* self)
{
  JsProxy_clear(self);
  Py_TYPE(self)->tp_free(self);
  return;
}

/**
 * repr overload, does `obj.toString()` which produces a low-quality repr.
 */
static PyObject*
JsProxy_Repr(PyObject* self)
{
  JsVal repr = _PyJsvObject_toString(JsProxy_VAL(self));
  if (JsvNull_Check(repr)) {
    return NULL;
  }
  return _Py_js2python(repr);
}

EM_JS_VAL(JsVal, _PyJsProxy_GetAttr_js, (JsVal jsobj, const char* ptrkey), {
  const jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  const result = jsobj[jskey];
  // clang-format off
  if (result === undefined && !(jskey in jsobj)) {
    // clang-format on
    return Module.error;
  }
  return result;
});

/**
 * getattr overload, first checks whether the attribute exists in the JsProxy
 * dict, and if so returns that. Otherwise, it attempts lookup on the wrapped
 * object.
 */
static PyObject*
JsProxy_GetAttr(PyObject* self, PyObject* attr)
{
  PyObject* result = _PyObject_GenericGetAttrWithDict(self, attr, NULL, 1);
  if (result != NULL || PyErr_Occurred()) {
    return result;
  }

  bool success = false;
  JsVal jsresult = JS_ERROR;
  // result:
  PyObject* pyresult = NULL;

  const char* key = PyUnicode_AsUTF8(attr);
  FAIL_IF_NULL(key);
  if (strcmp(key, "keys") == 0 && _PyJsvArray_Check(JsProxy_VAL(self))) {
    // Sometimes Python APIs test for the existence of a "keys" function
    // to decide whether something should be treated like a dict.
    // This mixes badly with the javascript Array.keys API, so pretend that it
    // doesn't exist. (Array.keys isn't very useful anyways so hopefully this
    // won't confuse too many people...)
    PyErr_SetString(PyExc_AttributeError, key);
    FAIL();
  }

  jsresult = _PyJsProxy_GetAttr_js(JsProxy_VAL(self), key);
  if (_PyJsvError_Check(jsresult)) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_AttributeError, key);
    }
    FAIL();
  }
  if (_PyJsvFunction_Check(jsresult)) {
    pyresult =
      _PyJsProxy_create_with_this(jsresult, JsProxy_VAL(self));
  } else {
    pyresult = _Py_js2python(jsresult);
  }
  FAIL_IF_NULL(pyresult);

  success = true;
finally:
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}


// clang-format off
EM_JS_NUM(int,
_PyJsProxy_SetAttr_js,
(JsVal jsobj, const char* ptrkey, JsVal jsval),
{
  let jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  jsobj[jskey] = jsval;
});

EM_JS_NUM(int, _PyJsProxy_DelAttr_js, (JsVal jsobj, const char* ptrkey), {
  let jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  delete jsobj[jskey];
});

/**
 * setattr / delattr overload.
 */
static int
JsProxy_SetAttr(PyObject* self, PyObject* attr, PyObject* pyvalue)
{
  bool success = false;

  const char* key = PyUnicode_AsUTF8(attr);
  FAIL_IF_NULL(key);

  if (strncmp(key, "__", 2) == 0) {
    // Avoid creating reference loops between Python and JavaScript with js
    // modules. Such reference loops make it hard to avoid leaking memory.
    if (strcmp(key, "__loader__") == 0 || strcmp(key, "__name__") == 0 ||
        strcmp(key, "__package__") == 0 || strcmp(key, "__path__") == 0 ||
        strcmp(key, "__spec__") == 0) {
      return PyObject_GenericSetAttr(self, attr, pyvalue);
    }
  }

  if (pyvalue == NULL) {
    FAIL_IF_MINUS_ONE(_PyJsProxy_DelAttr_js(JsProxy_VAL(self), key));
  } else {
    JsVal jsvalue = _Py_python2js(pyvalue);
    FAIL_IF_MINUS_ONE(_PyJsProxy_SetAttr_js(JsProxy_VAL(self), key, jsvalue));
  }

  success = true;
finally:
  return success ? 0 : -1;
}

EM_JS_BOOL(bool, _PyJsProxy_Bool_js, (JsVal val), {
  if (!val) {
    return false;
  }
  // We want to return false on container types with size 0.
  if (val.size === 0) {
    if(/HTML[A-Za-z]*Element/.test(getTypeTag(val))){
      // HTMLSelectElement and HTMLInputElement can have size 0 but we still
      // want to return true.
      return true;
    }
    // I think other things with a size are container types.
    return false;
  }
  if (val.length === 0 && _PyJsvArray_Check(val)) {
    return false;
  }
  if (val.byteLength === 0) {
    return false;
  }
  return true;
});

EM_JS_VAL(JsVal, _PyJsProxy_Dir_js, (JsVal jsobj), {
  let result = [];
  do {
    // clang-format off
    const names = Object.getOwnPropertyNames(jsobj);
    result.push(...names.filter(
      s => {
        let c = s.charCodeAt(0);
        return c < 48 || c > 57; /* Filter out integer array indices */
      }
    )
    // If the word is a reserved word followed by 0 or more underscores, add an
    // extra underscore to reverse the transformation applied by normalizeReservedWords.
    .map(word => isReservedWord(word.replace(/_*$/, "")) ? word + "_" : word));
    // clang-format on
  } while (jsobj = Object.getPrototypeOf(jsobj));
  return result;
});

/*[clinic input]
_pyodide_core.JsProxy.__dir__

Implementation for dir(proxy).

Walk the prototype chain of the object and adds the ownPropertyNames
of each prototype.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsProxy___dir___impl(PyObject *self)
/*[clinic end generated code: output=20e9b0e512f6084c input=4b450a7907328a4b]*/
{
  bool success = false;
  PyObject* object__dir__ = NULL;
  PyObject* keys = NULL;
  PyObject* result_set = NULL;
  JsVal jsdir = JS_ERROR;
  PyObject* pydir = NULL;

  PyObject* result = NULL;

  // First get base __dir__ via object.__dir__(self)
  // Would have been nice if they'd supplied PyObject_GenericDir...
  object__dir__ =
    PyObject_GetAttr((PyObject*)&PyBaseObject_Type, &_Py_ID(__dir__));
  FAIL_IF_NULL(object__dir__);
  keys = PyObject_CallOneArg(object__dir__, self);
  FAIL_IF_NULL(keys);
  result_set = PySet_New(keys);
  FAIL_IF_NULL(result_set);

  // Now get attributes of js object
  jsdir = _PyJsProxy_Dir_js(JsProxy_VAL(self));
  pydir = _Py_js2python(jsdir);
  FAIL_IF_NULL(pydir);
  // Merge and sort
  FAIL_IF_MINUS_ONE(_PySet_Update(result_set, pydir));
  if (_PyJsvArray_Check(JsProxy_VAL(self))) {
    // See comment about Array.keys in GetAttr
    FAIL_IF_MINUS_ONE(PySet_Discard(result_set, &_Py_ID(keys)));
  }
  result = PyList_New(0);
  FAIL_IF_NULL(result);
  FAIL_IF_MINUS_ONE(PyList_Extend(result, result_set));
  FAIL_IF_MINUS_ONE(PyList_Sort(result));

  success = true;
finally:
  Py_CLEAR(object__dir__);
  Py_CLEAR(keys);
  Py_CLEAR(result_set);
  Py_CLEAR(pydir);
  if (!success) {
    Py_CLEAR(result);
  }
  return result;
}

/*[clinic input]
_pyodide_core.JsProxy.to_py

    *

    depth: int = -1
        Limit the depth of the conversion. If a shallow conversion is
        desired, set ``depth`` to 1.

    default_converter: object = None

        If present, this will be invoked whenever Pyodide does not have some
        built in conversion for the object. If ``default_converter`` raises
        an error, the error will be allowed to propagate. Otherwise, the
        object returned will be used as the conversion.
        ``default_converter`` takes three arguments. The first argument is
        the value to be converted.

Convert the JsProxy to a native Python object.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsProxy_to_py_impl(PyObject *self, int depth,
                                 PyObject *default_converter)
/*[clinic end generated code: output=7b40b513d77caad8 input=a1c5cada6a5f5fb2]*/
{
  JsVal default_converter_js = Jsv_undefined;
  if (!Py_IsNone(default_converter)) {
    default_converter_js = _Py_python2js(default_converter);
  }
  PyObject* result =
    _Py_js2python_convert(JsProxy_VAL(self), depth, default_converter_js);
  if (_PyProxy_Check(default_converter_js)) {
    _PyProxy_Destroy(default_converter_js, NULL);
  }
  return result;
}

/**
 * Overload for bool(proxy), implemented for every JsProxy. Return `False` if
 * the object is falsey in JavaScript, or if it has a `size` field equal to 0,
 * or if it has a `length` field equal to zero and is an array. Otherwise return
 * `True`. This last convention could be replaced with "has a length equal to
 * zero and is not a function". In JavaScript, `func.length` returns the number
 * of arguments `func` expects. We definitely don't want 0-argument functions to
 * be falsey.
 */
static int
JsProxy_Bool(PyObject* self)
{
  return _PyJsProxy_Bool_js(JsProxy_VAL(self));
}

static PyObject*
JsProxy_js_id(PyObject* self, void* _unused)
{
  PyObject* result = NULL;

  JsRef idval = JsProxy_REF(self);
  int x[2] = { (int)Py_TYPE(self), (int)idval };
  Py_hash_t result_c = Py_HashBuffer(x, sizeof(x));
  FAIL_IF_MINUS_ONE(result_c);
  result = PyLong_FromLong(result_c);
finally:
  return result;
}

static PyObject*
JsProxy_RichCompare(PyObject* a, PyObject* b, int op)
{
  if (!_PyJsProxy_Check(b)) {
    switch (op) {
      case Py_EQ:
        Py_RETURN_FALSE;
      case Py_NE:
        Py_RETURN_TRUE;
      default:
        Py_RETURN_NOTIMPLEMENTED;
    }
  }

  int result;
  JsVal jsa = JsProxy_VAL(a);
  JsVal jsb = JsProxy_VAL(b);
  switch (op) {
    case Py_EQ:
      result = _PyJsv_equal(jsa, jsb);
      break;
    case Py_NE:
      result = _PyJsv_not_equal(jsa, jsb);
      break;
    default:
      Py_RETURN_NOTIMPLEMENTED;
  }

  if (result) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

static int
JsProxy_cinit(PyObject* obj, JsVal val)
{
  JsProxy* self = (JsProxy*)obj;
  self->js = hiwire_new_deduplicate(val);
#ifdef DEBUG_F
  extern bool tracerefs;
  if (tracerefs) {
    printf("JsProxy cinit: %zd, object: %zd\n", (long)obj, (long)self->js);
  }
#endif
  return 0;
}

EM_JS_VAL(JsVal, _PyJsProxy_GetIter_js, (JsVal obj), {
  return obj[Symbol.iterator]();
});

/**
 * iter overload. Present if IS_ITERABLE but not IS_ITERATOR (if the IS_ITERATOR
 * flag is present we use PyObject_SelfIter). Does `obj[Symbol.iterator]()`.
 */
static PyObject*
JsProxy_GetIter(PyObject* self)
{
  JsVal iter = _PyJsProxy_GetIter_js(JsProxy_VAL(self));
  FAIL_IF_JS_ERROR(iter);
  return js2python_pyjson(iter, JsProxy_is_py_json(self));
finally:
  return NULL;
}

// clang-format off
EM_JS_NUM(
JsVal,
_Py_handle_next_result_js,
(JsVal res, int* done, char** msg),
{
  let errmsg;
  if (typeof res !== "object") {
    errmsg = `Result should have type "object" not "${typeof res}"`;
  } else if (typeof res.done === "undefined") {
    if (typeof res.then === "function") {
      errmsg = `Result was a promise, use anext() / asend() / athrow() instead.`;
    } else {
      errmsg = `Result has no "done" field.`;
    }
  }
  if (errmsg) {
    ASSIGN_U32(msg, 0, stringToNewUTF8(errmsg));
    ASSIGN_U32(done, 0, -1);
  }
  ASSIGN_U32(done, 0, res.done);
  return res.value;
});

static PySendResult
handle_next_result(JsVal next_res, PyObject** result, bool pyjson){
  PySendResult res = PYGEN_ERROR;
  char* msg = NULL;
  *result = NULL;
  int done;

  JsVal jsresult = _Py_handle_next_result_js(next_res, &done, &msg);
  // done:
  //   1 ==> finished
  //   0 ==> not finished
  //  -1 ==> error (if msg is set, we set the error flag to a TypeError with
  //         msg otherwise the error flag must already be set)
  if (msg) {
    PyErr_SetString(PyExc_TypeError, msg);
    free(msg);
    FAIL();
  }
  FAIL_IF_MINUS_ONE(done);
  // If there was no "value", "idresult" will be jsundefined
  // so pyvalue will be set to Py_None.
  *result = _Py_js2python_immutable(jsresult);
  if (!*result) {
    *result = JsProxy_create_pyjson(jsresult, pyjson);
  }
  FAIL_IF_NULL(*result);

  res = done ? PYGEN_RETURN : PYGEN_NEXT;
finally:
  return res;
}

// clang-format on

static PySendResult
JsProxy_am_send(PyObject* self, PyObject* arg, PyObject** result)
{
  *result = NULL;
  PySendResult ret = PYGEN_ERROR;

  JsVal jsarg = Jsv_undefined;
  if (arg) {
    jsarg = _Py_python2js(arg);
    FAIL_IF_JS_ERROR(jsarg);
  }
  JsVal next_res =
    _PyJsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_next, jsarg);
  FAIL_IF_JS_ERROR(next_res);
  ret = handle_next_result(next_res, result, JsProxy_is_py_json(self));
finally:
  return ret;
}

static PyObject*
JsProxy_IterNext(PyObject* self)
{
  PyObject* result;
  if (JsProxy_am_send(self, NULL, &result) == PYGEN_RETURN) {
    // The Python docs for tp_iternext say "When the iterator is exhausted, it
    // must return NULL; a StopIteration exception may or may not be set."
    // So if the result is None, we can just leave error flag unset.
    if (!Py_IsNone(result)) {
      _PyGen_SetStopIterationValue(result);
    }
    Py_CLEAR(result);
  }
  return result;
}

/*[clinic input]
_pyodide_core.JsGenerator.send

    arg: object
    /

[clinic start generated code]*/

static PyObject *
_pyodide_core_JsGenerator_send(PyObject *self, PyObject *arg)
/*[clinic end generated code: output=ad7bc372b3c8b95d input=96e32bf91e18b7c7]*/
{
  PyObject* result;
  if (JsProxy_am_send(self, arg, &result) == PYGEN_RETURN) {
    if (Py_IsNone(result)) {
      PyErr_SetNone(PyExc_StopIteration);
    } else {
      _PyGen_SetStopIterationValue(result);
    }
    Py_CLEAR(result);
  }
  return result;
}

/**
 * Shared logic between throw and async throw.
 *
 * Possibly "typ" is an exception instance and val and tb are null. Otherwise,
 * it's an old style call "typ" should be an exception type, "val" an instance,
 * and tb an optional traceback. Figure out which is the case and get an
 * exception object.
 *
 * Then if the exception object is PyExc_GeneratorExit, call jsobj.return().
 * Otherwise, convert it to js and call jsobj.throw(jsexc). Return the result of
 * whichever of these two calls we make (or set the error flag and return NULL
 * if something goes wrong).
 */
JsVal
process_throw_args(PyObject* self, PyObject* typ, PyObject* val, PyObject* tb)
{
  if (Py_IsNone(tb)) {
    tb = NULL;
  } else if (tb != NULL && !PyTraceBack_Check(tb)) {
    PyErr_SetString(PyExc_TypeError,
                    "throw() third argument must be a traceback object");
    return JS_ERROR;
  }

  Py_INCREF(typ);
  Py_XINCREF(val);
  Py_XINCREF(tb);

  if (PyExceptionClass_Check(typ)) {
    PyErr_NormalizeException(&typ, &val, &tb);
    if (tb != NULL) {
      PyException_SetTraceback(val, tb);
    }
  } else if (PyExceptionInstance_Check(typ)) {
    /* Raising an instance.  The value should be a dummy. */
    if (val && !Py_IsNone(val)) {
      PyErr_SetString(PyExc_TypeError,
                      "instance exception may not have a separate value");
      goto failed_throw;
    } else {
      /* Normalize to raise <class>, <instance> */
      Py_XDECREF(val);
      val = typ;
      typ = PyExceptionInstance_Class(typ);
      Py_INCREF(typ);

      if (tb == NULL)
        /* Returns NULL if there's no traceback */
        tb = PyException_GetTraceback(val);
    }
  } else {
    /* Not something you can raise.  throw() fails. */
    PyErr_Format(PyExc_TypeError,
                 "exceptions must be classes or instances "
                 "deriving from BaseException, not %s",
                 Py_TYPE(typ)->tp_name);
    goto failed_throw;
  }

  PyErr_Restore(typ, val, tb);
  JsVal res;
  if (PyErr_ExceptionMatches(PyExc_GeneratorExit)) {
    PyErr_Clear();
    Js_IDENTIFIER(return);
    res = _PyJsvObject_CallMethodId_NoArgs(JsProxy_VAL(self), &JsId_return);
  } else {
    JsVal exc;
    static PyObject* JsException = NULL;
    if (JsException == NULL) {
      JsException = (PyObject*)JsProxy_get_subtype(IS_ERROR);
    }
    if (PyErr_ExceptionMatches(JsException)) {
      PyErr_Fetch(&typ, &val, &tb);
      exc = JsProxy_VAL(val);
      Py_CLEAR(typ);
      Py_CLEAR(val);
      Py_CLEAR(tb);
    } else {
      exc = _Py_pythonexc2js(); // cannot fail.
    }
    Js_IDENTIFIER(throw);
    res = _PyJsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_throw, exc);
  }
  return res;

failed_throw:
  /* Didn't use our arguments, so restore their original refcounts */
  Py_DECREF(typ);
  Py_XDECREF(val);
  Py_XDECREF(tb);
  return JS_ERROR;
}

static PyObject* JsGenerator_throw_inner(PyObject *self, PyObject *value,
                                         PyObject *val, PyObject *tb) {
  PyObject* result = NULL;
  JsVal throw_res = process_throw_args(self, value, val, tb);
  FAIL_IF_JS_ERROR(throw_res);
  PySendResult ret = handle_next_result(throw_res, &result, false);
  if (ret == PYGEN_RETURN) {
    if (Py_IsNone(result)) {
      PyErr_SetNone(PyExc_StopIteration);
    } else {
      _PyGen_SetStopIterationValue(result);
    }
    Py_CLEAR(result);
  }
finally:
  return result;
}

/*[clinic input]
_pyodide_core.JsGenerator.throw

    value: object

    val: object = NULL

    tb: object = NULL

    /
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsGenerator_throw_impl(PyObject *self, PyObject *value,
                                     PyObject *val, PyObject *tb)
/*[clinic end generated code: output=ce7a3dd3a2574fe4 input=730d7acdaa276e2d]*/
{
  return JsGenerator_throw_inner(self, value, val, tb);
}

/*[clinic input]
_pyodide_core.JsGenerator.close
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsGenerator_close_impl(PyObject *self)
/*[clinic end generated code: output=067ae8aa317142a6 input=9b72fd638cd3155f]*/
{
  PyObject* result =
    JsGenerator_throw_inner(self, PyExc_GeneratorExit, NULL, NULL);
  if (result != NULL) {
    // We could also just return it, but this matches Python. Generators that do
    // shenanigans stuff in "finally" blocks are hard to work with so we might
    // as well yell at people for using them.
    PyErr_SetString(PyExc_RuntimeError, "JavaScript generator ignored return");
    Py_DECREF(result);
    return NULL;
  }
  if (PyErr_ExceptionMatches(PyExc_StopIteration) ||
      PyErr_ExceptionMatches(PyExc_GeneratorExit)) {
    PyErr_Clear(); /* ignore these errors */
    Py_RETURN_NONE;
  }
  return NULL;
}


// A helper method for jsproxy_subscript.
EM_JS_VAL(JsVal, _PyJsProxy_subscript_js, (JsVal obj, JsVal key), {
  let result = obj.get(key);
  // clang-format off
  if (result === undefined) {
    // Try to distinguish between undefined and missing:
    // If the object has a "has" method and it returns false for this key, the
    // key is missing. Otherwise, assume key present and value was undefined.
    // TODO: in absence of a "has" method, should we return None or KeyError?
    if (obj.has && typeof obj.has === "function" && !obj.has(key)) {
      return Module.error;
    }
  }
  // clang-format on
  return result;
});

/**
 * __getitem__ for JsProxies that have a "get" method. Translates proxy[key] to
 * obj.get(key). Controlled by HAS_GET
 */
static PyObject*
JsProxy_subscript(PyObject* self, PyObject* pyidx)
{
  JsVal idx = _Py_python2js(pyidx);
  FAIL_IF_JS_ERROR(idx);
  JsVal result = _PyJsProxy_subscript_js(JsProxy_VAL(self), idx);
  if (_PyJsvError_Check(result)) {
    if (!PyErr_Occurred()) {
      PyErr_SetObject(PyExc_KeyError, pyidx);
    }
    FAIL();
  }
  return _Py_js2python(result);
finally:
  return NULL;
}


/**
 * __setitem__ / __delitem__ for JsProxies that have a "set" method (it's
 * assumed that they'll also have a del method...). Translates `proxy[key] =
 * value` to `obj.set(key, value)` and `del proxy[key]` to `obj.del(key)`.
 * Controlled by HAS_SET.
 */
static int
JsProxy_ass_subscript(PyObject* self, PyObject* pyidx, PyObject* pyvalue)
{
  bool success = false;

  JsVal idx = _Py_python2js(pyidx);
  FAIL_IF_JS_ERROR(idx);
  if (pyvalue == NULL) {
    Js_IDENTIFIER(delete);
    JsVal result =
      _PyJsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_delete, idx);
    FAIL_IF_JS_ERROR(result);
    if (!_PyJsv_to_bool(result)) {
      if (!PyErr_Occurred()) {
        PyErr_SetObject(PyExc_KeyError, pyidx);
      }
      FAIL();
    }
  } else {
    JsVal value = _Py_python2js(pyvalue);
    FAIL_IF_JS_ERROR(value);
    Js_IDENTIFIER(set);
    FAIL_IF_JS_ERROR(
      _PyJsvObject_CallMethodId_TwoArgs(JsProxy_VAL(self), &JsId_set, idx, value));
  }
  success = true;
finally:
  return success ? 0 : -1;
}


/*
 * Overload of the "in" operator for objects with a "has" method.
 * Translates `key in proxy` to `obj.has(key)`.
 * Controlled by HAS_HAS.
 */
static int
JsProxy_has(JsProxy* self, PyObject* obj)
{
  int result = -1;
  JsVal jsobj = _Py_python2js(obj);
  FAIL_IF_JS_ERROR(jsobj);
  Js_IDENTIFIER(has);
  JsVal jsresult =
    _PyJsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_has, jsobj);
  FAIL_IF_JS_ERROR(jsresult);
  result = _PyJsv_to_bool(jsresult);

finally:
  return result;
}

/**
 * Overload of the "in" operator for objects with an "includes" method.
 * Translates `key in proxy` to `obj.includes(key)`. We prefer to use
 * JsProxy_has when the object has both an `includes` and a `has` method.
 * Controlled by HAS_INCLUDES.
 */
static int
JsProxy_includes(JsProxy* self, PyObject* obj)
{
  int result = -1;
  JsVal jsobj = _Py_python2js(obj);
  FAIL_IF_JS_ERROR(jsobj);
  Js_IDENTIFIER(includes);
  JsVal jsresult =
    _PyJsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_includes, jsobj);
  FAIL_IF_JS_ERROR(jsresult);
  result = _PyJsv_to_bool(jsresult);

finally:
  return result;
}

EM_JS_VAL(JsVal, JsMap_GetIter_js, (JsVal obj), {
  let result;
  // clang-format off
  if(typeof obj.keys === 'function') {
    // clang-format on
    result = obj.keys();
  } else {
    result = obj[Symbol.iterator]();
  }
  return result;
})

/**
 * iter overload for maps. Present if IS_ITERABLE but not IS_ITERATOR (if the
 * IS_ITERATOR flag is present we use PyObject_SelfIter).
 * Prefers to iterate using map.keys() over map[Symbol.iterator]().
 */
static PyObject*
JsMap_GetIter(PyObject* self)
{
  JsVal iter = JsMap_GetIter_js(JsProxy_VAL(self));
  FAIL_IF_JS_ERROR(iter);
  return _Py_js2python(iter);
finally:
  return NULL;
}

/*[clinic input]
_pyodide_core.JsMap.keys
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_keys_impl(PyObject *self)
/*[clinic end generated code: output=7ed2af5c68741228 input=7126f4035073a0cb]*/
{
  return PyObject_CallMethodOneArg(collections_abc, &_Py_ID(KeysView), self);
}

/*[clinic input]
_pyodide_core.JsMap.values
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_values_impl(PyObject *self)
/*[clinic end generated code: output=1d0da65e8c96f35c input=1001f65975e32683]*/
{
  return PyObject_CallMethodOneArg(collections_abc, &_Py_ID(ValuesView), self);
}

/*[clinic input]
_pyodide_core.JsMap.items
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_items_impl(PyObject *self)
/*[clinic end generated code: output=5e9548c7fac354d1 input=54315017f32f6cec]*/
{
  return PyObject_CallMethodOneArg(collections_abc, &_Py_ID(ItemsView), self);
}

/*[clinic input]
_pyodide_core.JsMap.get

    key: object
    default: object = None

Return the value for key if key is in the dictionary, else default.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_get_impl(PyObject *self, PyObject *key,
                             PyObject *default_value)
/*[clinic end generated code: output=b0d6609942fe2a74 input=7dc3639e7f7eb096]*/
{
  PyObject* result = PyObject_GetItem(self, key);
  if (result != NULL) {
    return result;
  }
  PyErr_Clear();
  Py_INCREF(default_value);
  return default_value;
}

/*[clinic input]
_pyodide_core.JsMap.pop

    key: object
    default: object = NULL
    /

Return the value for key if key is in the Map, else default.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_pop_impl(PyObject *self, PyObject *key,
                             PyObject *default_value)
/*[clinic end generated code: output=4d52a2e1bb5e4406 input=5f8ceaddb1636240]*/
{
  PyObject* result = PyObject_GetItem(self, key);
  if (result == NULL) {
    if (default_value == NULL) {
      return NULL;
    } else {
      PyErr_Clear();
      Py_INCREF(default_value);
      return default_value;
    }
  }
  if (PyObject_DelItem(self, key) == -1) {
    Py_CLEAR(result);
    return NULL;
  }
  return result;
}

/*[clinic input]
_pyodide_core.JsMap.popitem

Remove and return a (key, value) pair as a 2-tuple.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_popitem_impl(PyObject *self)
/*[clinic end generated code: output=73aa024a982f3b99 input=af4ebd4eec73fa65]*/
{
  return PyObject_CallMethodOneArg(MutableMapping, &_Py_ID(popitem), self);
}

EM_JS_NUM(int, JsMap_clear_js, (JsVal map), {
  // clang-format off
  if(map && typeof map.clear === "function") {
    // clang-format on
    map.clear();
    return 1;
  }
  return 0;
})

/*[clinic input]
_pyodide_core.JsMap.clear

Remove all items from the dict.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_clear_impl(PyObject *self)
/*[clinic end generated code: output=128878a918c5ef68 input=a7bc2c8e1f846ed9]*/
{
  // If the map has a JavaScript "clear" function, use that.
  int status = JsMap_clear_js(JsProxy_VAL(self));
  if (status == -1) {
    return NULL;
  }
  if (status) {
    Py_RETURN_NONE;
  }
  // Otherwise iterate the map and delete the entries one at a time.
  return PyObject_CallMethodOneArg(MutableMapping, &_Py_ID(clear), self);
}

PyObject*
JsMap_update(JsProxy* self, PyObject* args, PyObject* kwds)
{
  PyObject* arg = NULL;
  if (!PyArg_ParseTuple(args, "|O:update", &arg)) {
    return NULL;
  }
  if (arg != NULL) {
    PyObject* status = PyObject_CallMethodObjArgs(
      MutableMapping, &_Py_ID(update), self, arg, NULL);
    if (status == NULL) {
      return NULL;
    }
    Py_CLEAR(status);
  }
  if (kwds != NULL) {
    PyObject* status = PyObject_CallMethodObjArgs(
      MutableMapping, &_Py_ID(update), self, arg, NULL);
    if (status == NULL) {
      return NULL;
    }
    Py_CLEAR(status);
  }
  Py_RETURN_NONE;
}

static PyMethodDef JsMap_update_MethodDef = {
  "update",
  (PyCFunction)JsMap_update,
  METH_VARARGS | METH_KEYWORDS,
};

/*[clinic input]
_pyodide_core.JsMap.setdefault

    key: object
    default: object = None
    /

Insert key with a value of default if key is not in the Map.

Return the value for key if key is in the Map, else default.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsMap_setdefault_impl(PyObject *self, PyObject *key,
                                    PyObject *default_value)
/*[clinic end generated code: output=fecfdc61d67be71b input=f4d6c0d8aa105875]*/
{
  PyObject* result = PyObject_GetItem(self, key);
  if (result != NULL) {
    return result;
  }
  PyErr_Clear();
  if (PyObject_SetItem(self, key, default_value) == -1) {
    return NULL;
  }
  Py_INCREF(default_value);
  return default_value;
}

#define ERR_NO_LENGTH -2
#define ERR_NEGATIVE_LENGTH -3
#define ERR_LENGTH_TOO_BIG -4

EM_JS_NUM(int, _Py_jsarray_get_length_helper, (JsVal val), {
  // clang-format off
  let result;
  // Maybe we should allow it to return a BigInt?
  if (typeof val.size === "number") {
    result = val.size;
  } else if (typeof val.length === "number") {
    result = val.length;
  } else {
    return ERR_NO_LENGTH;
  }
  if (result < 0) {
    return ERR_NEGATIVE_LENGTH;
  }
  if (result > INT_MAX) {
    return ERR_LENGTH_TOO_BIG;
  }
  return result;
  // clang-format on
});

// Needed to render the length accurately when there is an error
EM_JS_REF(char*, _Py_jsarray_get_length_string, (JsVal val), {
  let result;
  // clang-format off
  if (typeof val.size === "number") {
    result = val.size;
  } else if (typeof val.length === "number") {
    result = val.length;
  }
  // clang-format on
  return stringToNewUTF8(" " + result.toString())
})

static int
get_length(JsVal obj)
{
  int result = _Py_jsarray_get_length_helper(obj);
  if (result >= 0) {
    return result;
  }
  // Something went wrong. Case work:
  // * Either `val.size` or `val.length` was a getter which managed to raise an
  //   error. Propagate this JS error.
  if (result == -1) {
    return -1;
  }

  // Doesn't have a length or size, or the typeof the returned value is not
  // number
  if (result == ERR_NO_LENGTH) {
    PyErr_SetString(PyExc_TypeError, "object does not have a valid length");
    return -1;
  }

  char* length_as_string_alloc = _Py_jsarray_get_length_string(obj);
  char* length_as_string = length_as_string_alloc;
  if (length_as_string == NULL) {
    // Really screwed up.
    length_as_string = "";
  }
  if (result == ERR_NEGATIVE_LENGTH) {
    PyErr_Format(
      PyExc_ValueError, "length%s of object is negative", length_as_string);
  }
  if (result == ERR_LENGTH_TOO_BIG) {
    PyErr_Format(PyExc_OverflowError,
                 "length%s of object is larger than INT_MAX (%d)",
                 length_as_string,
                 INT_MAX);
  }
  if (length_as_string_alloc != NULL) {
    free(length_as_string_alloc);
  }
  return -1;
}

/**
 * len(proxy) overload for proxies of Js objects with `length` or `size` fields.
 * Prefers `object.size` over `object.length`. Controlled by HAS_LENGTH.
 */
static Py_ssize_t
JsProxy_length(PyObject* self)
{
  return get_length(JsProxy_VAL(self));
}


/**
 * __getitem__ for proxies of Js Arrays, controlled by IS_ARRAY
 */
static PyObject*
JsArray_subscript(PyObject* self, PyObject* item)
{
  PyObject* pyresult = NULL;

  if (PyIndex_Check(item)) {
    Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1)
      FAIL_IF_ERR_OCCURRED();
    if (i < 0) {
      int length = get_length(JsProxy_VAL(self));
      FAIL_IF_MINUS_ONE(length);
      i += length;
    }
    JsVal jsresult = _PyJsvArray_Get(JsProxy_VAL(self), i);
    if (_PyJsvError_Check(jsresult)) {
      if (!PyErr_Occurred()) {
        PyErr_SetObject(PyExc_IndexError, item);
      }
      FAIL();
    }
    pyresult = js2python_pyjson(jsresult, JsProxy_is_py_json(self));
    goto success;
  }
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step;
    FAIL_IF_MINUS_ONE(PySlice_Unpack(item, &start, &stop, &step));
    int length = get_length(JsProxy_VAL(self));
    FAIL_IF_MINUS_ONE(length);
    // PySlice_AdjustIndices is "Always successful" per the docs.
    Py_ssize_t slicelength = PySlice_AdjustIndices(length, &start, &stop, step);
    JsVal jsresult;
    if (slicelength <= 0) {
      jsresult = _PyJsvArray_New();
    } else {
      jsresult =
        _PyJsvArray_slice(JsProxy_VAL(self), slicelength, start, stop, step);
    }
    FAIL_IF_JS_ERROR(jsresult);
    pyresult = js2python_pyjson(jsresult, JsProxy_is_py_json(self));
    goto success;
  }
  PyErr_Format(PyExc_TypeError,
               "list indices must be integers or slices, not %.200s",
               Py_TYPE(item)->tp_name);
success:
finally:
  return pyresult;
}


/**
 * __setitem__ and __delitem__ for proxies of Js Arrays, controlled by IS_ARRAY
 */
static int
JsArray_ass_subscript(PyObject* self, PyObject* item, PyObject* pyvalue)
{
  bool success = false;
  PyObject* seq = NULL;
  Py_ssize_t i;
  if (PySlice_Check(item)) {
    Py_ssize_t start, stop, step, slicelength;
    FAIL_IF_MINUS_ONE(PySlice_Unpack(item, &start, &stop, &step));
    int length = get_length(JsProxy_VAL(self));
    FAIL_IF_MINUS_ONE(length);
    // PySlice_AdjustIndices is "Always successful" per the docs.
    slicelength = PySlice_AdjustIndices(length, &start, &stop, step);

    if (pyvalue != NULL) {
      seq = PySequence_Fast(pyvalue, "must assign iterable to extended slice");
      FAIL_IF_NULL(seq);
    }
    if (pyvalue != NULL && step != 1 &&
        PySequence_Fast_GET_SIZE(seq) != slicelength) {
      PyErr_Format(PyExc_ValueError,
                   "attempt to assign sequence of "
                   "size %zd to extended slice of "
                   "size %zd",
                   PySequence_Fast_GET_SIZE(seq),
                   slicelength);
      FAIL();
    }
    if (pyvalue == NULL) {
      if (slicelength <= 0) {
        success = true;
        goto finally;
      }
      if (step < 0) {
        // We have to delete in backwards order so make sure step > 0.
        stop = start + 1;
        start = stop + step * (slicelength - 1) - 1;
        step = -step;
      }
      FAIL_IF_MINUS_ONE(_PyJsvArray_slice_assign(
        JsProxy_VAL(self), slicelength, start, stop, step, 0, NULL));
    } else {
      if (step != 1 && !slicelength) {
        // At this point, assigning to an extended slice of length 0 must be a
        // no-op
        success = true;
        goto finally;
      }
      FAIL_IF_MINUS_ONE(_PyJsvArray_slice_assign(JsProxy_VAL(self),
                                              slicelength,
                                              start,
                                              stop,
                                              step,
                                              PySequence_Fast_GET_SIZE(seq),
                                              PySequence_Fast_ITEMS(seq)));
    }
    success = true;
    goto finally;
  } else if (PyIndex_Check(item)) {
    i = PyNumber_AsSsize_t(item, PyExc_IndexError);
    if (i == -1)
      FAIL_IF_ERR_OCCURRED();
    if (i < 0) {
      int length = get_length(JsProxy_VAL(self));
      FAIL_IF_MINUS_ONE(length);
      i += length;
    }
  } else {
    PyErr_Format(PyExc_TypeError,
                 "list indices must be integers or slices, not %.200s",
                 Py_TYPE(item)->tp_name);
    return -1;
  }

  if (pyvalue == NULL) {
    if (_PyJsvError_Check(_PyJsvArray_Delete(JsProxy_VAL(self), i))) {
      if (!PyErr_Occurred()) {
        PyErr_SetObject(PyExc_IndexError, item);
      }
      FAIL();
    }
  } else {
    JsVal jsvalue = _Py_python2js(pyvalue);
    FAIL_IF_JS_ERROR(jsvalue);
    FAIL_IF_MINUS_ONE(_PyJsvArray_Set(JsProxy_VAL(self), i, jsvalue));
  }
  success = true;
finally:
  Py_CLEAR(seq);
  return success ? 0 : -1;
}

PyObject*
JsArray_sq_item(PyObject* o, Py_ssize_t i)
{
  PyObject* pyresult = NULL;

  JsVal jsresult = _PyJsvArray_Get(JsProxy_VAL(o), i);
  if (_PyJsvError_Check(jsresult)) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_IndexError, "array index out of range");
    }
    FAIL();
  }
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);
finally:
  return pyresult;
}

Py_ssize_t
JsArray_sq_ass_item(PyObject* o, Py_ssize_t i, PyObject* pyval)
{
  bool success = false;

  if (pyval == NULL) {
    // Delete
    JsVal jsval = _PyJsvArray_Delete(JsProxy_VAL(o), i);
    FAIL_IF_JS_ERROR(jsval);
    success = true;
    goto finally;
  }

  JsVal jsval = _Py_python2js(pyval);
  FAIL_IF_JS_ERROR(jsval);
  FAIL_IF_MINUS_ONE(_PyJsvArray_Set(JsProxy_VAL(o), i, jsval));

  success = true;
finally:
  return success ? 0 : -1;
}


static int
JsArray_extend_by_python_iterable(JsVal jsarray, PyObject* iterable)
{
  PyObject* it = NULL;
  bool success = false;

  if (PyList_CheckExact(iterable) || PyTuple_CheckExact(iterable)) {
    iterable = PySequence_Fast(iterable, "argument must be iterable");
    if (!iterable)
      return -1;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(iterable);
    if (n == 0) {
      /* short circuit when iterable is empty */
      success = true;
      goto finally;
    }
    /* note that we may still have self == iterable here for the
     * situation a.extend(a), but the following code works
     * in that case too.  Just make sure to resize self
     * before calling PySequence_Fast_ITEMS.
     */
    /* populate the end of self with iterable's items */
    PyObject** src = PySequence_Fast_ITEMS(iterable);
    for (int i = 0; i < n; i++) {
      JsVal jsval = _Py_python2js(src[i]);
      FAIL_IF_JS_ERROR(jsval);
      _PyJsvArray_Push(jsarray, jsval);
    }
  } else {
    Py_INCREF(iterable);
    it = PyObject_GetIter(iterable);
    PyObject* (*iternext)(PyObject*);
    iternext = *Py_TYPE(it)->tp_iternext;

    /* Run iterator to exhaustion. */
    for (;;) {
      PyObject* item = iternext(it);
      if (item == NULL) {
        if (PyErr_Occurred()) {
          if (PyErr_ExceptionMatches(PyExc_StopIteration))
            PyErr_Clear();
          else {
            FAIL();
          }
        }
        break;
      }
      JsVal jsval = _Py_python2js(item);
      FAIL_IF_JS_ERROR(jsval);
      _PyJsvArray_Push(jsarray, jsval);
    }
  }
  success = true;
finally:
  Py_CLEAR(it);
  return success ? 0 : -1;
}

EM_JS(void, _Py_jsarray_destroy_entries, (JsVal array), {
  for (let v of array) {
    // clang-format off
    try {
      if(typeof v.destroy === "function"){
          v.destroy();
      }
    } catch(e) {
      console.warn("Weird error:", e);
    }
    // clang-format on
  }
})

static PyObject *
JsArray_extend_meth(PyObject *self, PyObject *iterable)
{
  bool success = false;

  JsVal temp = _PyJsvArray_New();
  // Make sure that if anything goes wrong the original array stays unmodified
  FAIL_IF_MINUS_ONE(JsArray_extend_by_python_iterable(temp, iterable));
  _PyJsvArray_Extend(JsProxy_VAL(self), temp);
  success = true;
finally:
  if (!success) {
    _Py_jsarray_destroy_entries(temp);
  }
  if (success) {
    Py_RETURN_NONE;
  } else {
    return NULL;
  }
}

/*[clinic input]
_pyodide_core.JsArray.extend

    iterable: object
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_extend_impl(PyObject *self, PyObject *iterable)
/*[clinic end generated code: output=96844cb265ce679d input=95ba00fe2efb2647]*/
{
  return JsArray_extend_meth(self, iterable);
}

static PyObject*
JsArray_sq_concat(PyObject* self, PyObject* other)
{
  PyObject* pyresult = NULL;
  bool success = true;

  JsVal jsresult = _PyJsvArray_ShallowCopy(JsProxy_VAL(self));
  FAIL_IF_JS_ERROR(jsresult);
  pyresult = _Py_js2python(jsresult);
  FAIL_IF_NULL(pyresult);
  FAIL_IF_MINUS_ONE(
    JsArray_extend_by_python_iterable(JsProxy_VAL(pyresult), other));
finally:
  if (!success) {
    Py_CLEAR(pyresult);
  }
  return pyresult;
}

static PyObject*
JsArray_sq_inplace_concat(PyObject* self, PyObject* other)
{
  PyObject* result = JsArray_extend_meth(self, other);
  FAIL_IF_NULL(result);
  Py_DECREF(result);
  Py_INCREF(self);
  return self;
finally:
  return NULL;
}


EM_JS_VAL(JsVal, _PyJsArray_repeat_js, (JsVal o, Py_ssize_t count), {
  // clang-format off
  return Array.from({ length : count }, () => o).flat();
  // clang-format on
})

static PyObject*
JsArray_sq_repeat(PyObject* o, Py_ssize_t count)
{
  JsVal jsresult = _PyJsArray_repeat_js(JsProxy_VAL(o), count);
  FAIL_IF_JS_ERROR(jsresult);
  return _Py_js2python(jsresult);

finally:
  return NULL;
}

EM_JS_NUM(int, _PyJsArray_inplace_repeat_js, (JsVal o, Py_ssize_t count), {
  // clang-format off
  o.splice(0, o.length, ... Array.from({ length : count }, () => o).flat());
  // clang-format on
})

static PyObject*
JsArray_sq_inplace_repeat(PyObject* o, Py_ssize_t count)
{
  FAIL_IF_MINUS_ONE(_PyJsArray_inplace_repeat_js(JsProxy_VAL(o), count));
  Py_INCREF(o);
  return o;
finally:
  return NULL;
}

/*[clinic input]
_pyodide_core.JsArray.append

  arg: object
  /

Append object to the end of the list.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_append(PyObject *self, PyObject *arg)
/*[clinic end generated code: output=7bcbece121484fa8 input=24978f1a5efdb28f]*/
{
  bool success = false;

  JsVal jsarg = _Py_python2js(arg);
  FAIL_IF_JS_ERROR(jsarg);
  _PyJsvArray_Push(JsProxy_VAL(self), jsarg);

  success = true;
finally:
  if (success) {
    Py_RETURN_NONE;
  } else {
    return NULL;
  }
}

// Copied directly from Python
static inline int
valid_index(Py_ssize_t i, Py_ssize_t limit)
{
  /* The cast to size_t lets us use just a single comparison
      to check whether i is in the range: 0 <= i < limit.

      See:  Section 14.2 "Bounds Checking" in the Agner Fog
      optimization manual found at:
      https://www.agner.org/optimize/optimizing_cpp.pdf
  */
  return (size_t)i < (size_t)limit;
}

/*[clinic input]
_pyodide_core.JsArray.pop

  index: Py_ssize_t = -1
  /

Remove and return item at index (default last).

Raises IndexError if list is empty or index is out of range
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_pop_impl(PyObject *self, Py_ssize_t index)
/*[clinic end generated code: output=24746ba91016c701 input=6b7db7a1eed28077]*/
{
  PyObject* pyresult = NULL;
  PyObject* iobj = NULL;

  int length = get_length(JsProxy_VAL(self));
  FAIL_IF_MINUS_ONE(length);

  if (length == 0) {
    /* Special-case most common failure cause */
    PyErr_SetString(PyExc_IndexError, "pop from empty list");
    FAIL();
  }
  if (index < 0)
    index += length;
  if (!valid_index(index, length)) {
    PyErr_SetString(PyExc_IndexError, "pop index out of range");
    FAIL();
  }

  JsVal jsresult = _PyJsvArray_Delete(JsProxy_VAL(self), index);
  FAIL_IF_JS_ERROR(jsresult);
  pyresult = _Py_js2python(jsresult);

finally:
  Py_CLEAR(iobj);
  return pyresult;
}

EM_JS(JsVal, JsArray_reversed_iterator, (JsVal array), {
  return new ReversedIterator(array);
}
// clang-format off
class ReversedIterator {
  constructor(array) {
    this._array = array;
    this._i = array.length - 1;
  }

  __length_hint__() {
    return this._array.length;
  }

  [Symbol.toStringTag]() {
    return "ReverseIterator";
  }

  next() {
    const i = this._i;
    const a = this._array;
    const done = i < 0;
    const value = done ? undefined : a[i];
    this._i--;
    return { done, value };
  }
}
// clang-format on
)

/*[clinic input]
_pyodide_core.JsArray.__reversed__

Return a reverse iterator over the array.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray___reversed___impl(PyObject *self)
/*[clinic end generated code: output=22c20f23b9bfb1db input=bcdeea00699fdd70]*/
{
  JsVal iter = JsArray_reversed_iterator(JsProxy_VAL(self));
  FAIL_IF_JS_ERROR(iter);
  return _Py_js2python(iter);
finally:
  return NULL;
}

// clang-format off
EM_JS_NUM(int,
_PyJsArray_index_js,
(JsVal o, JsVal v, int start, int stop),
{
  for (let i = start; i < stop; i++) {
    if (o[i] === v) {
      return i;
    }
  }
  return -2;
})
// clang-format on

static Py_ssize_t
JsArray_index_helper(PyObject* self,
                     PyObject* value,
                     Py_ssize_t start,
                     Py_ssize_t stop)
{
  Py_ssize_t length = JsProxy_length(self);
  if (length == -1) {
    return -1;
  }
  if (start < 0) {
    start += length;
    if (start < 0)
      start = 0;
  }
  if (stop < 0) {
    stop += length;
    if (stop < 0)
      stop = 0;
  }
  if (stop > length) {
    stop = length;
  }

  JsVal jsvalue = _Py_python2js_track_proxies(value, JS_ERROR, true);
  if (_PyJsvError_Check(jsvalue)) {
    PyErr_Clear();
    for (Py_ssize_t i = start; i < stop; i++) {
      JsVal jsobj = _PyJsvArray_Get(JsProxy_VAL(self), i);
      // We know `value` is not a `JsProxy`: if it were we would have taken the
      // other branch. Thus, if `jsobj` is not a `PyProxy`,
      // `PyObject_RichCompareBool` is guaranteed to return false. As a speed
      // up, only perform the check if the object is a `PyProxy`.
      PyObject* pyobj = _PyProxy_AsPyObject(jsobj); /* borrowed! */
      if (pyobj == NULL) {
        continue;
      }
      int cmp = PyObject_RichCompareBool(pyobj, value, Py_EQ);
      if (cmp > 0)
        return i;
      else if (cmp < 0)
        goto error;
    }
    goto error;
  } else {
    int result = _PyJsArray_index_js(JsProxy_VAL(self), jsvalue, start, stop);
    if (result == -2) {
      goto error;
    }
    return result;
  }
error:
  PyErr_Format(PyExc_ValueError, "%R is not in list", value);
  return -1;
}

/*[clinic input]
_pyodide_core.JsArray.index

    value: object
    start: slice_index(accept={int}) = 0
    stop: slice_index(accept={int}, c_default="PY_SSIZE_T_MAX") = sys.maxsize
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_index_impl(PyObject *self, PyObject *value,
                                 Py_ssize_t start, Py_ssize_t stop)
/*[clinic end generated code: output=3101601bb5f177cb input=ac9c80dc07baad8a]*/
{
  Py_ssize_t result = JsArray_index_helper(self, value, start, stop);
  if (result == -1) {
    return NULL;
  }
  return PyLong_FromSsize_t(result);
}

EM_JS_NUM(int,
_PyJsArray_count_js,
(JsVal o, JsVal v),
{
  let result = 0;
  for (let i = 0; i < o.length; i++) {
    if (o[i] === v) {
      result++;
    }
  }
  return result;
})

/*[clinic input]
_pyodide_core.JsArray.count

    value: object

Return number of occurrences of value.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_count_impl(PyObject *self, PyObject *value)
/*[clinic end generated code: output=f04bbb791389d160 input=cd9142e54226ffb8]*/
{
  JsVal jsvalue = _Py_python2js_track_proxies(value, JS_ERROR, true);
  if (_PyJsvError_Check(jsvalue)) {
    PyErr_Clear();
    int result = 0;
    Py_ssize_t stop = JsProxy_length(self);
    if (stop == -1) {
      return NULL;
    }
    for (int i = 0; i < stop; i++) {
      JsVal jsobj = _PyJsvArray_Get(JsProxy_VAL(self), i);
      // We know `value` is not a `JsProxy`: if it were we would have taken the
      // other branch. Thus, if `jsobj` is not a `PyProxy`,
      // `PyObject_RichCompareBool` is guaranteed to return false. As a speed
      // up, only perform the check if the object is a `PyProxy`.
      PyObject* pyobj = _PyProxy_AsPyObject(jsobj); /* borrowed! */
      if (pyobj == NULL) {
        continue;
      }
      int cmp = PyObject_RichCompareBool(pyobj, value, Py_EQ);
      if (cmp > 0)
        result++;
      else if (cmp < 0)
        return NULL;
    }
    return PyLong_FromSsize_t(result);
  } else {
    int result = _PyJsArray_count_js(JsProxy_VAL(self), jsvalue);
    if (result == -1) {
      return NULL;
    } else {
      return PyLong_FromSsize_t(result);
    }
  }
}

EM_JS_NUM(int, _PyJsArray_reverse_js, (JsVal array), { array.reverse(); })

/*[clinic input]
_pyodide_core.JsArray.reverse

Return number of occurrences of value.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_reverse_impl(PyObject *self)
/*[clinic end generated code: output=b319f0c2394d499e input=a7b03428f72d214e]*/
{
  if (_PyJsArray_reverse_js(JsProxy_VAL(self)) == -1) {
    return NULL;
  }
  Py_RETURN_NONE;
}

/*[clinic input]
_pyodide_core.JsArray.insert

    index: Py_ssize_t
    object: object
    /

Insert object before index.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_insert_impl(PyObject *self, Py_ssize_t index,
                                  PyObject *object)
/*[clinic end generated code: output=255daa10da5ca6f4 input=65739aa26dcdbe37]*/
{
  JsVal jsvalue = _Py_python2js(object);
  FAIL_IF_JS_ERROR(jsvalue);
  FAIL_IF_MINUS_ONE(_PyJsvArray_Insert(JsProxy_VAL(self), index, jsvalue));
  Py_RETURN_NONE;
finally:
  return NULL;
}

/*[clinic input]
_pyodide_core.JsArray.remove

     value: object
     /

Remove first occurrence of value.

Raises ValueError if the value is not present.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsArray_remove(PyObject *self, PyObject *value)
/*[clinic end generated code: output=ef6fba8da3e93b77 input=d4a7480ba04102b1]*/
{
  int index = JsArray_index_helper(self, value, 0, PY_SSIZE_T_MAX);
  FAIL_IF_MINUS_ONE(index);
  _PyJsvArray_Delete(JsProxy_VAL(self), index);
  Py_RETURN_NONE;
finally:
  return NULL;
}

////////////////////////////////////////////////////////////
// JsMethod
//
// A subclass of JsProxy for methods

/**
 * __call__ overload for methods. Controlled by IS_CALLABLE.
 */
static PyObject*
JsMethod_Vectorcall(PyObject* self,
                    PyObject* const* pyargs,
                    size_t nargsf,
                    PyObject* kwnames)
{
  return _PyJsMethod_Vectorcall_impl(JsProxy_VAL(self),
                                  JsMethod_THIS(self),
                                  pyargs,
                                  nargsf,
                                  kwnames);
}

/**
 * jsproxy.new implementation. Controlled by IS_CALLABLE.
 *
 * This does Reflect.construct(this, args). In other words, this treats the
 * JsMethod as a JavaScript class, constructs a new JavaScript object of that
 * class and returns a new JsProxy wrapping it. Similar to `new this(args)`.
 */
static PyObject*
JsMethod_Construct(PyObject* self,
                   PyObject* const* pyargs,
                   Py_ssize_t nargs,
                   PyObject* kwnames)
{
  return _PyJsMethod_Construct_impl(
    JsProxy_VAL(self), pyargs, nargs, kwnames);
}

// clang-format off
static PyMethodDef JsMethod_Construct_MethodDef = {
  "new",
  (PyCFunction)JsMethod_Construct,
  METH_FASTCALL | METH_KEYWORDS
};
// clang-format on

static PyObject*
JsMethod_descr_get(PyObject* self, PyObject* obj, PyObject* type)
{
  PyObject* result = NULL;

  if (Py_IsNone(obj) || obj == NULL) {
    Py_INCREF(self);
    return self;
  }

  JsVal jsobj = _Py_python2js(obj);
  FAIL_IF_JS_ERROR(jsobj);
  result = _PyJsProxy_create_with_this(JsProxy_VAL(self), jsobj);

finally:
  return result;
}

static int
JsMethod_cinit(PyObject* self, JsVal this_)
{
  JsMethod_THIS_REF(self) = _PyJsRef_new(this_);
  JsMethod_VECTORCALL(self) = JsMethod_Vectorcall;
  return 0;
}

////////////////////////////////////////////////////////////
// JsMethod
//
// A subclass of JsProxy for errors

/*[clinic input]
_pyodide_core.JsException.__reduce__
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsException___reduce___impl(PyObject *self)
/*[clinic end generated code: output=e18df4ef67cb9ddf input=01774b928904de24]*/
{
  // Record name, message, and stack.
  // See _core_docs.JsException._new_exc where the unpickling will happen.
  PyObject* res = NULL;
  PyObject* args = NULL;
  PyObject* name = NULL;
  PyObject* message = NULL;
  PyObject* stack = NULL;

  name = PyObject_GetAttrString(self, "name");
  FAIL_IF_NULL(name);
  message = PyObject_GetAttrString(self, "message");
  FAIL_IF_NULL(message);
  stack = PyObject_GetAttrString(self, "stack");
  FAIL_IF_NULL(stack);

  args = PyTuple_Pack(3, name, message, stack);
  FAIL_IF_NULL(args);

  PyObject* dict = JsProxy_DICT(self);
  if (dict) {
    res = PyTuple_Pack(3, Py_TYPE(self), args, dict);
  } else {
    res = PyTuple_Pack(2, Py_TYPE(self), args);
  }

finally:
  Py_CLEAR(args);
  Py_CLEAR(name);
  Py_CLEAR(message);
  Py_CLEAR(stack);
  return res;
}

// clang-format off
EM_JS_VAL(JsVal,
JsException_new_helper,
(char* name_ptr, char* message_ptr, char* stack_ptr),
{
  let name = UTF8ToString(name_ptr);
  let message = UTF8ToString(message_ptr);
  let stack = UTF8ToString(stack_ptr);
  return API.deserializeError(name, message, stack);
});
// clang-format on

// We use this to unpickle JsException objects.
static PyObject*
JsException_new(PyTypeObject* subtype, PyObject* args, PyObject* kwds)
{
  static char* kwlist[] = { "name", "message", "stack", 0 };
  char* name;
  char* message = "";
  char* stack = "";
  if (!PyArg_ParseTupleAndKeywords(
        args, kwds, "s|ss:__new__", kwlist, &name, &message, &stack)) {
    return NULL;
  }
  JsVal result = JsException_new_helper(name, message, stack);
  FAIL_IF_JS_ERROR(result);
  return _Py_js2python(result);
finally:
  return NULL;
}

static int
JsException_init(PyBaseExceptionObject* self, PyObject* args, PyObject* kwds)
{
  return 0;
}

EM_JS_REF(PyObject*, JsDoubleProxy_unwrap_js, (JsVal id), {
  return Module.PyProxy_getPtr(id);
});

/*[clinic input]
_pyodide_core.JsDoubleProxy.unwrap

Unwrap a double proxy created with create_proxy.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsDoubleProxy_unwrap_impl(PyObject *self)
/*[clinic end generated code: output=082ded3e691dfd38 input=a1ebbeedfb1c9ed6]*/
{
  PyObject* result = JsDoubleProxy_unwrap_js(JsProxy_VAL(self));
  Py_XINCREF(result);
  return result;
}

/*[clinic input]
_pyodide_core.JsProxy.as_py_json

Returns a new JsProxy that treats a JavaScript object as Python json.

It allows one to treat a JavaScript object that is a mixture of
JavaScript arrays and objects as a mixture of Python lists and dicts.
[clinic start generated code]*/

static PyObject *
_pyodide_core_JsProxy_as_py_json_impl(PyObject *self)
/*[clinic end generated code: output=f5714861b7b1656d input=a896600d4ec0052c]*/
{
  int flags = JsProxy_getflags(self);
  if (flags & IS_ARRAY) {
    flags |= IS_PY_JSON_SEQUENCE;
  } else {
    flags |= IS_PY_JSON_DICT;
  }
  return JsProxy_create_with_type(
    flags, JsProxy_VAL(self), JsMethod_THIS(self));
}

EM_JS_VAL(JsVal, _PyJsObjMap_GetIter_js, (JsVal obj), {
  return iterObject(obj);
})

static PyObject*
JsObjMap_GetIter(PyObject* self)
{
  JsVal iter = _PyJsObjMap_GetIter_js(JsProxy_VAL(self));
  FAIL_IF_JS_ERROR(iter);
  return _Py_js2python(iter);
finally:
  return NULL;
}

EM_JS_NUM(int, _PyJsObjMap_length_js, (JsVal obj), {
  let length = 0;
  for (let _ of iterObject(obj)) {
    length++;
  }
  return length;
})

static int
JsObjMap_length(PyObject* self)
{
  return _PyJsObjMap_length_js(JsProxy_VAL(self));
}

// A helper method for JsObjMap_subscript.
EM_JS_VAL(JsVal, _PyJsObjMap_subscript_js, (JsVal obj, JsVal key), {
  if (!Object.prototype.hasOwnProperty.call(obj, key)) {
    return Module.error;
  }
  return obj[key];
});

static PyObject*
JsObjMap_subscript(PyObject* self, PyObject* pyidx)
{
  if (!PyUnicode_Check(pyidx)) {
    PyErr_SetObject(PyExc_KeyError, pyidx);
    return NULL;
  }

  PyObject* pyresult = NULL;

  JsVal key = _Py_python2js(pyidx);
  FAIL_IF_JS_ERROR(key);
  JsVal result = _PyJsObjMap_subscript_js(JsProxy_VAL(self), key);
  if (_PyJsvError_Check(result)) {
    if (!PyErr_Occurred()) {
      PyErr_SetObject(PyExc_KeyError, pyidx);
    }
    FAIL();
  }
  pyresult = _Py_js2python_immutable(result);
  if (pyresult == NULL) {
    pyresult = JsProxy_create_pyjson(result, true);
  }

finally:
  return pyresult;
}

// A helper method for JsObjMap_ass_subscript.
// clang-format off
EM_JS_NUM(int,
_PyJsObjMap_ass_subscript_js,
(JsVal obj, JsVal key, JsVal value),
{
  if(value === Module.error) {
    if (!Object.prototype.hasOwnProperty.call(obj, key)) {
      return -1;
    }
    delete obj[key];
  } else {
    obj[key] = value;
  }
  return 0;
});
// clang-format on

static int
JsObjMap_ass_subscript(PyObject* self, PyObject* pykey, PyObject* pyvalue)
{
  if (!PyUnicode_Check(pykey)) {
    if (pyvalue) {
      PyErr_SetString(
        PyExc_TypeError,
        "Can only assign keys of type string to JavaScript object map");
    } else {
      PyErr_SetObject(PyExc_KeyError, pykey);
    }
    return -1;
  }

  bool success = false;
  JsVal value = JS_ERROR;
  JsVal key = _Py_python2js(pykey);
  if (pyvalue != NULL) {
    value = _Py_python2js(pyvalue);
    FAIL_IF_JS_ERROR(value);
  }
  int status = _PyJsObjMap_ass_subscript_js(JsProxy_VAL(self), key, value);
  if (status == -1) {
    if (!PyErr_Occurred()) {
      PyErr_SetObject(PyExc_KeyError, pykey);
    }
    FAIL();
  }
  success = true;
finally:
  return success ? 0 : -1;
}

EM_JS_NUM(int, _PyJsObjMap_contains_js, (JsVal obj, JsVal key), {
  return Object.prototype.hasOwnProperty.call(obj, key);
});

static int
JsObjMap_contains(PyObject* self, PyObject* obj)
{
  if (!PyUnicode_Check(obj)) {
    // All keys are strings or symbols so if it's not a string don't check.
    // TODO: maybe support symbols??
    return 0;
  }
  JsVal jsobj = _Py_python2js(obj);
  FAIL_IF_JS_ERROR(jsobj);
  return _PyJsObjMap_contains_js(JsProxy_VAL(self), jsobj);

finally:
  return -1;
}

// clang-format off
static PyNumberMethods JsProxy_NumberMethods = {
  .nb_bool = JsProxy_Bool
};
// clang-format on

static PyGetSetDef JsProxy_GetSet[] = { { "js_id", .get = JsProxy_js_id },
                                        { NULL } };

static PyTypeObject JsProxyType = {
  .tp_name = "pyodide.ffi.JsProxy",
  .tp_basicsize = sizeof(JsProxy),
  .tp_dealloc = (destructor)JsProxy_dealloc,
  .tp_getattro = JsProxy_GetAttr,
  .tp_setattro = JsProxy_SetAttr,
  .tp_richcompare = JsProxy_RichCompare,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "A proxy to make a Javascript object behave like a Python object",
  .tp_as_number = &JsProxy_NumberMethods,
  .tp_repr = JsProxy_Repr,
  .tp_dictoffset = offsetof(JsProxy, dict),
  .tp_getset = JsProxy_GetSet,
};

/**
 * This dynamically creates a subtype of JsProxy using PyType_FromSpecWithBases.
 * It is called from JsProxy_get_subtype(flags) when a type with the given flags
 * doesn't already exist.
 *
 * None of these types have tp_new method, we create them with tp_alloc and then
 * call whatever init methods are needed. "new" and multiple inheritance don't
 * go together very well.
 */
static PyObject*
JsProxy_create_subtype(int flags, bool is_py_json)
{
  // Make sure these stack allocations are large enough to fit!
  PyType_Slot slots[20];
  int cur_slot = 0;
  PyMethodDef methods[50];
  int cur_method = 0;
  PyMemberDef members[5];
  int cur_member = 0;

  int tp_flags = Py_TPFLAGS_DEFAULT;

  bool objmap = (flags & IS_PY_JSON_DICT);
  int mapping_flags = HAS_GET | HAS_LENGTH | IS_ITERABLE;
  bool mapping = (flags & mapping_flags) == mapping_flags;
  bool mutable_mapping = mapping && (flags & HAS_SET);
  mapping = mapping || objmap;
  mutable_mapping = mutable_mapping || objmap;

  char* type_name = "pyodide.ffi.JsProxy";
  int basicsize = sizeof(JsProxy);

  #define AddMethods(to_add...)                       \
    do {                                              \
      PyMethodDef meths_array[] = { to_add {0} };     \
      PyMethodDef *meths = meths_array;               \
      while (meths->ml_name != NULL) {                \
        methods[cur_method++] = *meths;               \
        meths++;                                      \
      }                                               \
    } while(0)                                        \

  AddMethods(
    _PYODIDE_CORE_JSPROXY___DIR___METHODDEF
    _PYODIDE_CORE_JSPROXY_TO_PY_METHODDEF
  );
  if (mapping) {
    AddMethods(
      _PYODIDE_CORE_JSMAP_KEYS_METHODDEF
      _PYODIDE_CORE_JSMAP_VALUES_METHODDEF
      _PYODIDE_CORE_JSMAP_ITEMS_METHODDEF
      _PYODIDE_CORE_JSMAP_GET_METHODDEF
    );
  }
  if (mutable_mapping) {
    AddMethods(
      _PYODIDE_CORE_JSMAP_POP_METHODDEF
      _PYODIDE_CORE_JSMAP_POPITEM_METHODDEF
      _PYODIDE_CORE_JSMAP_CLEAR_METHODDEF
      _PYODIDE_CORE_JSMAP_SETDEFAULT_METHODDEF
      JsMap_update_MethodDef,
    );
  }
  if (objmap) {
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_iter, .pfunc = (void*)JsObjMap_GetIter };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_mp_length, .pfunc = (void*)JsObjMap_length };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_subscript,
                                       .pfunc = (void*)JsObjMap_subscript };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_ass_subscript,
                                       .pfunc = (void*)JsObjMap_ass_subscript };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_sq_contains,
                                       .pfunc = (void*)JsObjMap_contains };
    goto skip_container_slots;
  }

  if (flags & HAS_GET) {
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_subscript,
                                       .pfunc = (void*)JsProxy_subscript };
    tp_flags |= Py_TPFLAGS_MAPPING;
  }
  if (flags & HAS_SET) {
    // It's assumed that if HAS_SET then also HAS_DELETE.
    // We will try to use `obj.delete("key")` to resolve `del proxy["key"]`
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_ass_subscript,
                                       .pfunc = (void*)JsProxy_ass_subscript };
  }
  if (flags & HAS_HAS) {
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_contains, .pfunc = (void*)JsProxy_has };
  }
  if ((flags & HAS_INCLUDES) && !(flags & HAS_HAS)) {
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_contains, .pfunc = (void*)JsProxy_includes };
  }
  if (flags & HAS_LENGTH) {
    // If the function has a `size` or `length` member, use this for
    // `len(proxy)` Prefer `size` to `length`.
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_mp_length, .pfunc = (void*)JsProxy_length };
  }

  if (flags & IS_ARRAY) {
    // If the object is an array (or a HTMLCollection or NodeList), then we want
    // subscripting `proxy[idx]` to go to `jsobj[idx]` instead of
    // `jsobj.get(idx)`. Hopefully anyone else who defines a custom array object
    // will subclass Array.
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_subscript,
                                       .pfunc = (void*)JsArray_subscript };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_mp_ass_subscript,
                                       .pfunc = (void*)JsArray_ass_subscript };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_inplace_concat,
                     .pfunc = (void*)JsArray_sq_inplace_concat };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_concat, .pfunc = (void*)JsArray_sq_concat };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_repeat, .pfunc = (void*)JsArray_sq_repeat };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_inplace_repeat,
                     .pfunc = (void*)JsArray_sq_inplace_repeat };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_length, .pfunc = (void*)JsProxy_length };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_sq_item, .pfunc = (void*)JsArray_sq_item };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_sq_ass_item,
                                       .pfunc = (void*)JsArray_sq_ass_item };
    AddMethods(
      _PYODIDE_CORE_JSARRAY___REVERSED___METHODDEF
      _PYODIDE_CORE_JSARRAY_APPEND_METHODDEF
      _PYODIDE_CORE_JSARRAY_COUNT_METHODDEF
      _PYODIDE_CORE_JSARRAY_EXTEND_METHODDEF
      _PYODIDE_CORE_JSARRAY_INDEX_METHODDEF
      _PYODIDE_CORE_JSARRAY_INSERT_METHODDEF
      _PYODIDE_CORE_JSARRAY_POP_METHODDEF
      _PYODIDE_CORE_JSARRAY_REMOVE_METHODDEF
      _PYODIDE_CORE_JSARRAY_REVERSE_METHODDEF
    );
  }

  if (flags & IS_GENERATOR) {
    // throw and close need "throw" and "return" methods to work. We currently
    // don't trust that an object with "next", "throw", and "return" is a
    // generator though -- we require that it actually have it's toStringTag set
    // to Generator.
    AddMethods(
      _PYODIDE_CORE_JSGENERATOR_THROW_METHODDEF
      _PYODIDE_CORE_JSGENERATOR_CLOSE_METHODDEF
    );
  }

  if ((flags & IS_ITERABLE) && !(flags & IS_ITERATOR)) {
    // If it is an iterator we should use SelfIter instead.
    if (mapping) {
      // Prefer `obj.keys()` over `obj[Symbol.iterator]()`
      slots[cur_slot++] =
        (PyType_Slot){ .slot = Py_tp_iter, .pfunc = (void*)JsMap_GetIter };
    } else {
      slots[cur_slot++] =
        (PyType_Slot){ .slot = Py_tp_iter, .pfunc = (void*)JsProxy_GetIter };
    }
  }

  // If it's an iterator, we aren't sure whether it is an async iterator or a
  // sync iterator -- they both define a next method, you have to see whether
  // the result is  a promise or not to learn whether we are async. But most
  // iterators also define `Symbol.iterator` to return themself, and most async
  // iterators define `Symbol.asyncIterator` to return themself. So if one of
  // these is defined but not the other, we use this to decide what type we are.

  // Iterator methods
  if (flags & IS_ITERATOR) {
    // We're not sure whether it is an async iterator or a sync iterator. So add
    // both methods and raise at runtime if someone uses the wrong one.
    // JsProxy_GetIter would work just as well as PyObject_SelfIter
    // but PyObject_SelfIter avoids an unnecessary allocation.
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_iter, .pfunc = (void*)PyObject_SelfIter };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_iternext, .pfunc = (void*)JsProxy_IterNext };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_am_send, .pfunc = (void*)JsProxy_am_send };
    AddMethods(_PYODIDE_CORE_JSGENERATOR_SEND_METHODDEF);
  }

skip_container_slots:

  if (flags & IS_CALLABLE) {
    tp_flags |= Py_TPFLAGS_HAVE_VECTORCALL;
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_call, .pfunc = (void*)PyVectorcall_Call };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_tp_descr_get,
                                       .pfunc = (void*)JsMethod_descr_get };
    AddMethods(JsMethod_Construct_MethodDef,);
    members[cur_member++] = (PyMemberDef){
      .name = "__vectorcalloffset__",
      .type = Py_T_PYSSIZET,
      .flags = Py_READONLY,
      .offset =
        offsetof(JsProxy, tf) + offsetof(struct CallableFields, vectorcall),
    };
  }

  if (flags & IS_ERROR) {
    type_name = "pyodide.ffi.JsException";
    AddMethods(_PYODIDE_CORE_JSEXCEPTION___REDUCE___METHODDEF);
    tp_flags |= Py_TPFLAGS_HAVE_GC;
    tp_flags |= Py_TPFLAGS_BASE_EXC_SUBCLASS;
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_traverse,
                     .pfunc =
                       (void*)((PyTypeObject*)PyExc_Exception)->tp_traverse };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_new, .pfunc = JsException_new };
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_init, .pfunc = JsException_init };
  }

  if (flags & IS_DOUBLE_PROXY) {
    AddMethods(_PYODIDE_CORE_JSDOUBLEPROXY_UNWRAP_METHODDEF);
  }
  if (INCLUDE_PYJSON_METHODS(flags)) {
    AddMethods(_PYODIDE_CORE_JSPROXY_AS_PY_JSON_METHODDEF);
  }

  members[cur_member++] = (PyMemberDef){ 0 };
  methods[cur_method++] = (PyMethodDef){ 0 };
  #undef AddMethods

  bool success = false;
  void* mem = NULL;
  PyObject* bases = NULL;
  PyObject* flags_obj = NULL;
  PyObject* result = NULL;

  // PyType_FromSpecWithBases copies "members" automatically into the end of the
  // type. It doesn't store the slots. But it just copies the pointer to
  // "methods" and "getsets" into the PyTypeObject, so if we give it stack
  // allocated methods or getsets there will be trouble. Instead, heap allocate
  // some memory and copy them over.
  //
  // If the type object were later deallocated, we would leak this memory. It's
  // unclear how to fix that, but we store the type in JsProxy_TypeDict forever
  // anyway so it will never be deallocated.
  mem = PyMem_Malloc(sizeof(PyMethodDef) * cur_method);
  PyMethodDef* methods_heap = (PyMethodDef*)mem;
  if (methods_heap == NULL) {
    PyErr_NoMemory();
    FAIL();
  }
  memcpy(methods_heap, methods, sizeof(PyMethodDef) * cur_method);

  slots[cur_slot++] =
    (PyType_Slot){ .slot = Py_tp_members, .pfunc = (void*)members };
  slots[cur_slot++] =
    (PyType_Slot){ .slot = Py_tp_methods, .pfunc = (void*)methods_heap };
  slots[cur_slot++] = (PyType_Slot){ 0 };

  // clang-format off
  PyType_Spec spec = {
    .name = type_name,
    .basicsize = basicsize,
    .itemsize = 0,
    .flags = tp_flags,
    .slots = slots,
  };
  if (flags & IS_ERROR) {
    bases = PyTuple_Pack(2, &JsProxyType, PyExc_Exception);
    FAIL_IF_NULL(bases);
    // The multiple inheritance we are doing is not recognized as legal by
    // Python:
    //
    // 1. the solid_base of JsProxy is JsProxy.
    // 2. the solid_base of Exception is BaseException.
    // 3. Neither issubclass(JsProxy, BaseException) nor
    //    issubclass(BaseException, JsProxy).
    // 4. If you use multiple inheritance, the sold_bases of the different bases
    //    are required to be totally ordered (otherwise Python assumes there is
    //    a memory layout clash).
    //
    // So Python concludes that there is a memory layout clash. However, we have
    // carefully ensured that the memory layout is okay (with the
    // _Static_assert's at the top of this file) so now we need to trick the
    // subclass creation algorithm.
    //
    // We temporarily set the mro of JsProxy to be (BaseException,) so that
    // issubclass(JsProxy, BaseException) returns True. This convinces
    // PyType_FromSpecWithBases that everything is okay. Once we have created
    // the type, we restore the mro.
    PyObject* save_mro = JsProxyType.tp_mro;
    JsProxyType.tp_mro = PyTuple_Pack(1, PyExc_BaseException);
    result = PyType_FromSpecWithBases(&spec, bases);
    Py_CLEAR(JsProxyType.tp_mro);
    JsProxyType.tp_mro = save_mro;
  } else {
    bases = PyTuple_Pack(1, &JsProxyType);
    FAIL_IF_NULL(bases);
    result = PyType_FromSpecWithBases(&spec, bases);
  }
  FAIL_IF_NULL(result);
  Py_SET_TYPE(result, (PyTypeObject*)JsProxy_metaclass);

  flags_obj = PyLong_FromLong(flags);
  FAIL_IF_NULL(flags_obj);
  FAIL_IF_MINUS_ONE(
    PyObject_SetAttr(result, &_Py_ID(_js_type_flags), flags_obj));

  success = true;
finally:
  if (!success) {
    Py_CLEAR(result);
  }
  if (!success && mem != NULL) {
    PyMem_Free(mem);
  }
  Py_CLEAR(bases);
  Py_CLEAR(flags_obj);
  return result;
}

static PyObject* JsProxy_TypeDict;

/**
 * Look up the appropriate type object in the types dict, if we don't find it
 * call JsProxy_create_subtype. This is a helper for JsProxy_create_with_this
 * and JsProxy_create.
 */
static PyTypeObject*
JsProxy_get_subtype(int flags)
{
  PyObject* flags_key = PyLong_FromLong(flags);
  PyObject* type = PyDict_GetItemWithError(JsProxy_TypeDict, flags_key);
  Py_XINCREF(type);
  if (type != NULL || PyErr_Occurred()) {
    goto finally;
  }
  type = JsProxy_create_subtype(flags, false);
  FAIL_IF_NULL(type);
  FAIL_IF_MINUS_ONE(PyDict_SetItem(JsProxy_TypeDict, flags_key, type));
finally:
  Py_CLEAR(flags_key);
  return (PyTypeObject*)type;
}

#define SET_FLAG_IF(flag, cond)                                                \
  if (cond) {                                                                  \
    type_flags |= flag;                                                        \
  }


#define SET_FLAG_IF_HAS_METHOD(flag, meth)                                     \
  SET_FLAG_IF(flag, hasMethod(obj, meth))


EM_JS_NUM(int, JsProxy_compute_typeflags, (JsVal obj, bool is_py_json), {
  let type_flags = 0;

  function safeCall(cb){
    try {
      return cb();
    } catch(e) {}
  }
  const typeTag = getTypeTag(obj);

  SET_FLAG_IF_HAS_METHOD(HAS_GET, "get")
  SET_FLAG_IF_HAS_METHOD(HAS_SET, "set");
  SET_FLAG_IF_HAS_METHOD(HAS_HAS, "has");
  SET_FLAG_IF_HAS_METHOD(HAS_INCLUDES, "includes");
  SET_FLAG_IF(HAS_LENGTH,
    (hasProperty(obj, "size")) ||
    (hasProperty(obj, "length") && typeof obj !== "function"));
  SET_FLAG_IF(IS_CALLABLE, typeof obj === "function");
  SET_FLAG_IF(IS_ARRAY, safeCall(() => Array.isArray(obj)));
  SET_FLAG_IF(IS_DOUBLE_PROXY, API.isPyProxy(obj));
  SET_FLAG_IF(IS_GENERATOR, typeTag === "[object Generator]");
  SET_FLAG_IF_HAS_METHOD(IS_ITERABLE, Symbol.iterator);
  SET_FLAG_IF(IS_ITERATOR, hasMethod(obj, "next") && (hasMethod(obj, Symbol.iterator) || !hasMethod(obj, Symbol.asyncIterator)));
  /**
   * DOMException is a weird special case. According to WHATWG, there are two
   * types of Exception objects, simple exceptions and DOMExceptions. The spec
   * says:
   *
   * > if an implementation gives native Error objects special powers or
   * > nonstandard properties (such as a stack property), it should also expose
   * > those on DOMException objects
   *
   * Firefox respects this and has DOMException.stack. But Safari and Chrome do
   * not. Hence the special check here for DOMException.
   */
  SET_FLAG_IF(IS_ERROR,
    (
      hasProperty(obj, "name")
      && hasProperty(obj, "message")
      && (
        hasProperty(obj, "stack")
        || constructorName === "DOMException"
      )
    ) && !(type_flags & (IS_CALLABLE)));

  if (is_py_json && (type_flags & (IS_ARRAY | IS_ITERATOR))) {
    // tagging IS_PY_JSON_SEQUENCE on IS_ITERATOR is a bit of a hack
    type_flags |= IS_PY_JSON_SEQUENCE;
  } else if (is_py_json && INCLUDE_PYJSON_METHODS(type_flags)) {
    type_flags |= IS_PY_JSON_DICT;
  }

  return type_flags;
});

static PyObject*
JsProxy_create_with_type(int type_flags,
                         JsVal object,
                         JsVal this)
{
  bool success = false;
  PyTypeObject* type = NULL;
  PyObject* result = NULL;

  type = JsProxy_get_subtype(type_flags);
  FAIL_IF_NULL(type);

  result = type->tp_alloc(type, 0);
  FAIL_IF_NONZERO(JsProxy_cinit(result, object));
  if (type_flags & IS_CALLABLE) {
    FAIL_IF_NONZERO(JsMethod_cinit(result, this));
  }
  if (type_flags & IS_ERROR) {
    PyObject* arg =
      JsProxy_create_with_type(type_flags & (~IS_ERROR), object, this);
    FAIL_IF_NULL(arg);
    PyObject* args = PyTuple_Pack(1, arg);
    Py_CLEAR(arg);
    FAIL_IF_NULL(args);
    JsException_ARGS(result) = args;
  }

  success = true;
finally:
  Py_CLEAR(type);
  if (!success) {
    Py_CLEAR(result);
  }
  return result;
}

/**
 * Create a JsProxy. In case it's a method, bind "this" to the argument. (In
 * most cases "this" will be NULL, `JsProxy_create` specializes to this case.)
 * We check what capabilities are present on the javascript object, set
 * appropriate flags, then we get the appropriate type with JsProxy_get_subtype.
 */
PyObject*
_PyJsProxy_create_with_this(JsVal object,
                            JsVal this)
{
  int type_flags = JsProxy_compute_typeflags(object, false);
  if (type_flags == -1) {
    PyErr_SetString(PyExc_SystemError,
                    "Internal error occurred in JsProxy_compute_typeflags");
    return NULL;
  }
  return JsProxy_create_with_type(type_flags, object, this);
}

EMSCRIPTEN_KEEPALIVE PyObject*
_PyJsProxy_create(JsVal object)
{
  return _PyJsProxy_create_with_this(object, JS_NULL);
}

static PyObject*
JsProxy_create_pyjson(JsVal object, bool pyjson)
{
  int typeflags = JsProxy_compute_typeflags(object, pyjson);
  return JsProxy_create_with_type(typeflags, object, JS_ERROR);
}

EMSCRIPTEN_KEEPALIVE bool
_PyJsProxy_Check(PyObject* x)
{
  return PyObject_TypeCheck(x, &JsProxyType);
}

JsVal
_PyJsProxy_Val(PyObject* x)
{
  return JsProxy_VAL(x);
}

static int
add_flag(PyObject* dict, char* name, int value)
{
  PyObject* value_py = NULL;
  bool success = false;

  value_py = PyLong_FromLong(value);
  FAIL_IF_NULL(value_py);
  FAIL_IF_MINUS_ONE(PyDict_SetItemString(dict, name, value_py));

  success = true;
finally:
  Py_CLEAR(value_py);
  return success ? 0 : -1;
}

int
_Py_jsproxy_init(PyObject* core_module)
{
  bool success = false;
  PyObject* flag_dict = NULL;
  PyObject* JsException = NULL;
  PyObject* _pyodide = NULL;

  _pyodide = PyImport_ImportModule("_pyodide");
  FAIL_IF_NULL(_pyodide);
  JsProxy_metaclass =
    PyObject_GetAttrString(_pyodide, "_JsProxyMetaClass");
  FAIL_IF_NULL(JsProxy_metaclass);

  FAIL_IF_MINUS_ONE(PyType_Ready(&JsProxyType));
  JsProxy_TypeDict = PyDict_New();
  FAIL_IF_NULL(JsProxy_TypeDict);
  FAIL_IF_MINUS_ONE(
    PyModule_AddObjectRef(core_module, "jsproxy_typedict", JsProxy_TypeDict));

  collections_abc = PyImport_ImportModule("collections.abc");
  FAIL_IF_NULL(collections_abc);
  MutableMapping = PyObject_GetAttr(collections_abc, &_Py_ID(MutableMapping));
  FAIL_IF_NULL(MutableMapping);

  flag_dict = PyObject_GetAttrString(_pyodide, "js_flags");
  FAIL_IF_NULL(flag_dict);

#define AddFlag(flag) FAIL_IF_MINUS_ONE(add_flag(flag_dict, #flag, flag))

  AddFlag(IS_ITERABLE);
  AddFlag(IS_ITERATOR);
  AddFlag(HAS_LENGTH);
  AddFlag(HAS_GET);
  AddFlag(HAS_SET);
  AddFlag(HAS_HAS);
  AddFlag(HAS_INCLUDES);
  AddFlag(IS_CALLABLE);
  AddFlag(IS_ARRAY);
  AddFlag(IS_DOUBLE_PROXY);
  AddFlag(IS_GENERATOR);
  AddFlag(IS_ERROR);
  AddFlag(IS_PY_JSON_DICT);
  AddFlag(IS_PY_JSON_SEQUENCE);

#undef AddFlag


  JsException = (PyObject*)JsProxy_get_subtype(IS_ERROR);
  FAIL_IF_NULL(JsException);
  FAIL_IF_MINUS_ONE(
    PyObject_SetAttrString(core_module, "JsException", JsException));

  success = true;
finally:
  Py_CLEAR(flag_dict);
  Py_CLEAR(JsException);
  return success ? 0 : -1;
}
