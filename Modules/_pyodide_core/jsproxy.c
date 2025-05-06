#include "jsproxy.h"
#include "jsproxy_call.h"
#include "jslib.h"
#include "error_handling.h"
#include "js2python.h"
#include "python2js.h"
#include "pyidentifier.h"

#define IS_CALLABLE        (1 << 0)

_Py_IDENTIFIER(_js_type_flags);


struct CallableFields
{
  JsRef this_;
  vectorcallfunc vectorcall;
};

typedef struct
{
  PyObject_HEAD
  PyObject* dict;
  union {
    struct CallableFields mf;
  } tf;
  JsRef js;
} JsProxy;


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
  PyMemberDef members[5];
  int cur_member = 0;

  int tp_flags = Py_TPFLAGS_DEFAULT;

  char* type_name = "pyodide.ffi.JsProxy";
  int basicsize = sizeof(JsProxy);

  if (flags & IS_CALLABLE) {
    tp_flags |= Py_TPFLAGS_HAVE_VECTORCALL;
    slots[cur_slot++] =
      (PyType_Slot){ .slot = Py_tp_call, .pfunc = (void*)PyVectorcall_Call };
    slots[cur_slot++] = (PyType_Slot){ .slot = Py_tp_descr_get,
                                       .pfunc = (void*)JsMethod_descr_get };
    members[cur_member++] = (PyMemberDef){
      .name = "__vectorcalloffset__",
      .type = Py_T_PYSSIZET,
      .flags = Py_READONLY,
      .offset =
        offsetof(JsProxy, tf) + offsetof(struct CallableFields, vectorcall),
    };
  }

  members[cur_member++] = (PyMemberDef){ 0 };

  bool success = false;
  PyObject* bases = NULL;
  PyObject* flags_obj = NULL;
  PyObject* result = NULL;

  slots[cur_slot++] =
    (PyType_Slot){ .slot = Py_tp_members, .pfunc = (void*)members };
  slots[cur_slot++] = (PyType_Slot){ 0 };

  // clang-format off
  PyType_Spec spec = {
    .name = type_name,
    .basicsize = basicsize,
    .itemsize = 0,
    .flags = tp_flags,
    .slots = slots,
  };
  bases = PyTuple_Pack(1, &JsProxyType);
  FAIL_IF_NULL(bases);
  result = PyType_FromSpecWithBases(&spec, bases);
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


EM_JS_NUM(int, JsProxy_compute_typeflags, (JsVal obj), {
  let type_flags = 0;

  SET_FLAG_IF(IS_CALLABLE, typeof obj === "function");
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
