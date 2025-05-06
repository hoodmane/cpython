#include "jsproxy.h"
#include "jsproxy_call.h"
#include "jslib.h"
#include "error_handling.h"
#include "js2python.h"
#include "python2js.h"
#include "jsmemops.h"

#include "pyidentifier.h"
#include "internal/pycore_genobject.h"
#include "pycore_setobject.h"     // _PySet_Update()

#define IS_CALLABLE        (1 << 0)
#define IS_ITERABLE        (1 << 1)
#define IS_ITERATOR        (1 << 2)
#define IS_ERROR           (1 << 3)

_Py_IDENTIFIER(_js_type_flags);
_Py_IDENTIFIER(__dir__);
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
#define JsMethod_THIS(x) JsRef_toVal(JsMethod_THIS_REF(x))
#define JsMethod_VECTORCALL(x) (((JsProxy*)x)->tf.mf.vectorcall)

int
JsProxy_getflags(PyObject* self)
{
  PyObject* pyflags =
    _PyObject_GetAttrId((PyObject*)Py_TYPE(self), &PyId__js_type_flags);
  if (pyflags == NULL) {
    return -1;
  }
  int result = PyLong_AsLong(pyflags);
  Py_CLEAR(pyflags);
  return result;
}

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
  JsVal repr = JsvObject_toString(JsProxy_VAL(self));
  if (JsvNull_Check(repr)) {
    return NULL;
  }
  return js2python(repr);
}

EM_JS(bool, isReservedWord, (int word), {
  return Module.pythonReservedWords.has(word);
}
Module.pythonReservedWords = new Set([
  "False",  "await", "else",     "import", "pass",   "None",    "break",
  "except", "in",    "raise",    "True",   "class",  "finally", "is",
  "return", "and",   "continue", "for",    "lambda", "try",     "as",
  "def",    "from",  "nonlocal", "while",  "assert", "del",     "global",
  "not",    "with",  "async",    "elif",   "if",     "or",      "yield",
]);
);

/**
 * action: a javascript string, one of get, set, or delete. For error reporting.
 * word: a javascript string, the property being accessed
 */
EM_JS(int, normalizeReservedWords, (int word), {
  // clang-format off
  // 1. if word is not a reserved word followed by 0 or more underscores, return
  //    it unchanged.
  const noTrailing_ = word.replace(/_*$/, "");
  if (!isReservedWord(noTrailing_)) {
    return word;
  }
  // 2. If there is at least one trailing underscore, return the word with a
  //    single underscore removed.
  if (noTrailing_ !== word) {
    return word.slice(0, -1);
  }
  // 3. If the word is exactly a reserved word, return it unchanged
  return word;
  // clang-format on
});

EM_JS_VAL(JsVal, JsProxy_GetAttr_js, (JsVal jsobj, const char* ptrkey), {
  const jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  const result = jsobj[jskey];
  // clang-format off
  if (result === undefined && !(jskey in jsobj)) {
    // clang-format on
    return null;
  }
  return nullToUndefined(result);
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
  JsVal jsresult = JS_NULL;
  // result:
  PyObject* pyresult = NULL;

  const char* key = PyUnicode_AsUTF8(attr);
  FAIL_IF_NULL(key);
  if (strcmp(key, "keys") == 0 && JsvArray_Check(JsProxy_VAL(self))) {
    // Sometimes Python APIs test for the existence of a "keys" function
    // to decide whether something should be treated like a dict.
    // This mixes badly with the javascript Array.keys API, so pretend that it
    // doesn't exist. (Array.keys isn't very useful anyways so hopefully this
    // won't confuse too many people...)
    PyErr_SetString(PyExc_AttributeError, key);
    FAIL();
  }

  jsresult = JsProxy_GetAttr_js(JsProxy_VAL(self), key);
  if (JsvNull_Check(jsresult)) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_AttributeError, key);
    }
    FAIL();
  }
  pyresult = js2python(jsresult);
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
JsProxy_SetAttr_js,
(JsVal jsobj, const char* ptrkey, JsVal jsval),
{
  let jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  jsobj[jskey] = jsval;
});
// clang-format on

EM_JS_NUM(int, JsProxy_DelAttr_js, (JsVal jsobj, const char* ptrkey), {
  let jskey = normalizeReservedWords(UTF8ToString(ptrkey));
  delete jsobj[jskey];
});

/**
 * setattr / delttr overload. TODO: Raise an error if the attribute exists on
 * the proxy.
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
    FAIL_IF_MINUS_ONE(JsProxy_DelAttr_js(JsProxy_VAL(self), key));
  } else {
    JsVal jsvalue = python2js(pyvalue);
    FAIL_IF_MINUS_ONE(JsProxy_SetAttr_js(JsProxy_VAL(self), key, jsvalue));
  }

  success = true;
finally:
  return success ? 0 : -1;
}

EM_JS_BOOL(bool, JsProxy_Bool_js, (JsVal val), {
  // clang-format off
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
  if (val.length === 0 && JsvArray_Check(val)) {
    return false;
  }
  if (val.byteLength === 0) {
    return false;
  }
  return true;
  // clang-format on
});

EM_JS_VAL(JsVal, JsProxy_Dir_js, (JsVal jsobj), {
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

/**
 * Overload of `dir(proxy)`. Walks the prototype chain of the object and adds
 * the ownPropertyNames of each prototype.
 */
static PyObject*
JsProxy_Dir(PyObject* self, PyObject* _args)
{
  bool success = false;
  PyObject* object__dir__ = NULL;
  PyObject* keys = NULL;
  PyObject* result_set = NULL;
  JsVal jsdir = JS_NULL;
  PyObject* pydir = NULL;
  PyObject* keys_str = NULL;

  PyObject* result = NULL;

  // First get base __dir__ via object.__dir__(self)
  // Would have been nice if they'd supplied PyObject_GenericDir...
  object__dir__ =
    _PyObject_GetAttrId((PyObject*)&PyBaseObject_Type, &PyId___dir__);
  FAIL_IF_NULL(object__dir__);
  keys = PyObject_CallOneArg(object__dir__, self);
  FAIL_IF_NULL(keys);
  result_set = PySet_New(keys);
  FAIL_IF_NULL(result_set);

  // Now get attributes of js object
  jsdir = JsProxy_Dir_js(JsProxy_VAL(self));
  pydir = js2python(jsdir);
  FAIL_IF_NULL(pydir);
  // Merge and sort
  FAIL_IF_MINUS_ONE(_PySet_Update(result_set, pydir));
  if (JsvArray_Check(JsProxy_VAL(self))) {
    // See comment about Array.keys in GetAttr
    keys_str = PyUnicode_FromString("keys");
    FAIL_IF_NULL(keys_str);
    FAIL_IF_MINUS_ONE(PySet_Discard(result_set, keys_str));
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
  Py_CLEAR(keys_str);
  if (!success) {
    Py_CLEAR(result);
  }
  return result;
}

static PyMethodDef JsProxy_Dir_MethodDef = {
  "__dir__",
  (PyCFunction)JsProxy_Dir,
  METH_NOARGS,
  PyDoc_STR("Returns a list of the members and methods on the object."),
};

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
  return JsProxy_Bool_js(JsProxy_VAL(self));
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
  if (!JsProxy_Check(b)) {
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
      result = Jsv_equal(jsa, jsb);
      break;
    case Py_NE:
      result = Jsv_not_equal(jsa, jsb);
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


EM_JS_VAL(JsVal, JsProxy_GetIter_js, (JsVal obj), {
  return obj[Symbol.iterator]();
});

/**
 * iter overload. Present if IS_ITERABLE but not IS_ITERATOR (if the IS_ITERATOR
 * flag is present we use PyObject_SelfIter). Does `obj[Symbol.iterator]()`.
 */
static PyObject*
JsProxy_GetIter(PyObject* self)
{
  JsVal iter = JsProxy_GetIter_js(JsProxy_VAL(self));
  FAIL_IF_JS_NULL(iter);
  return js2python(iter);
finally:
  return NULL;
}

// clang-format off
EM_JS_NUM(
JsVal,
handle_next_result_js,
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

PySendResult
handle_next_result(JsVal next_res, PyObject** result){
  PySendResult res = PYGEN_ERROR;
  char* msg = NULL;
  *result = NULL;
  int done;

  JsVal jsresult = handle_next_result_js(next_res, &done, &msg);
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
  *result = js2python(jsresult);
  FAIL_IF_NULL(*result);

  res = done ? PYGEN_RETURN : PYGEN_NEXT;
finally:
  return res;
}

// clang-format on

PySendResult
JsProxy_am_send(PyObject* self, PyObject* arg, PyObject** result)
{
  *result = NULL;
  PySendResult ret = PYGEN_ERROR;

  JsVal jsarg = Jsv_undefined;
  if (arg) {
    jsarg = python2js(arg);
    FAIL_IF_JS_NULL(jsarg);
  }
  JsVal next_res =
    JsvObject_CallMethodId_OneArg(JsProxy_VAL(self), &JsId_next, jsarg);
  FAIL_IF_JS_NULL(next_res);
  ret = handle_next_result(next_res, result);
finally:
  return ret;
}

PyObject*
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

PyObject*
JsGenerator_send(PyObject* self, PyObject* arg)
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

static PyMethodDef JsGenerator_send_MethodDef = {
  "send",
  (PyCFunction)JsGenerator_send,
  METH_O,
};

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
  return JsMethod_Vectorcall_impl(JsProxy_VAL(self),
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
  return JsMethod_Construct_impl(
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

  JsVal jsobj = python2js(obj);
  FAIL_IF_JS_NULL(jsobj);
  result = JsProxy_create_with_this(JsProxy_VAL(self), jsobj);

finally:
  return result;
}

static int
JsMethod_cinit(PyObject* self, JsVal this_)
{
  JsMethod_THIS_REF(self) = JsRef_new(this_);
  JsMethod_VECTORCALL(self) = JsMethod_Vectorcall;
  return 0;
}

////////////////////////////////////////////////////////////
// JsMethod
//
// A subclass of JsProxy for errors

static PyObject*
JsException_reduce(PyObject* self, PyObject* Py_UNUSED(ignored))
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

static PyMethodDef JsException_reduce_MethodDef = {
  "__reduce__",
  (PyCFunction)JsException_reduce,
  METH_NOARGS
};


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
  FAIL_IF_JS_NULL(result);
  return js2python(result);
finally:
  return NULL;
}

static int
JsException_init(PyBaseExceptionObject* self, PyObject* args, PyObject* kwds)
{
  return 0;
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
JsProxy_create_subtype(int flags)
{
  // Make sure these stack allocations are large enough to fit!
  PyType_Slot slots[20];
  int cur_slot = 0;
  PyMethodDef methods[50];
  int cur_method = 0;
  PyMemberDef members[5];
  int cur_member = 0;

  int tp_flags = Py_TPFLAGS_DEFAULT;

  char* type_name = "pyodide.ffi.JsProxy";
  int basicsize = sizeof(JsProxy);

  methods[cur_method++] = JsProxy_Dir_MethodDef;

  if ((flags & IS_ITERABLE) && !(flags & IS_ITERATOR)) {
    // If it is an iterator we should use SelfIter instead.
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_iter, .pfunc = (void*)JsProxy_GetIter };
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
    methods[cur_method++] = JsGenerator_send_MethodDef;
  }


  if (flags & IS_CALLABLE) {
    tp_flags |= Py_TPFLAGS_HAVE_VECTORCALL;
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_call, .pfunc = (void*)PyVectorcall_Call };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_tp_descr_get,
                                       .pfunc = (void*)JsMethod_descr_get };
    methods[cur_method++] = JsMethod_Construct_MethodDef;
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
    methods[cur_method++] = JsException_reduce_MethodDef;
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

  members[cur_member++] = (PyMemberDef){ 0 };
  methods[cur_method++] = (PyMethodDef){ 0 };

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

  flags_obj = PyLong_FromLong(flags);
  FAIL_IF_NULL(flags_obj);
  FAIL_IF_MINUS_ONE(
    PyObject_SetAttr(result, _PyUnicode_FromId(&PyId__js_type_flags), flags_obj));

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
  type = JsProxy_create_subtype(flags);
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


EM_JS_NUM(int, JsProxy_compute_typeflags, (JsVal obj), {
  let type_flags = 0;

  SET_FLAG_IF(IS_CALLABLE, typeof obj === "function");
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

  return type_flags;
});

PyObject*
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
JsProxy_create_with_this(JsVal object,
                         JsVal this)
{
  int type_flags = JsProxy_compute_typeflags(object);
  if (type_flags == -1) {
    PyErr_SetString(PyExc_SystemError,
                    "Internal error occurred in JsProxy_compute_typeflags");
    return NULL;
  }
  return JsProxy_create_with_type(type_flags, object, this);
}

PyObject*
JsProxy_create(JsVal object)
{
  return JsProxy_create_with_this(object, JS_NULL);
}

EMSCRIPTEN_KEEPALIVE bool
JsProxy_Check(PyObject* x)
{
  return PyObject_TypeCheck(x, &JsProxyType);
}

JsVal
JsProxy_Val(PyObject* x)
{
  return JsProxy_VAL(x);
}


int
jsproxy_init(PyObject* core_module)
{
  bool success = false;
  FAIL_IF_MINUS_ONE(PyType_Ready(&JsProxyType));
  JsProxy_TypeDict = PyDict_New();
  FAIL_IF_NULL(JsProxy_TypeDict);
  FAIL_IF_MINUS_ONE(
    PyModule_AddObjectRef(core_module, "jsproxy_typedict", JsProxy_TypeDict));

  success = true;
finally:
  return success ? 0 : -1;
}
