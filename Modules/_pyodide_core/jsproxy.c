#include "jsproxy.h"
#include "jslib.h"
#include "error_handling.h"
#include "js2python.h"
#include "python2js.h"

typedef struct
{
  PyObject_HEAD
  PyObject* dict;
  JsRef js;
} JsProxy;


#define JsProxy_REF(x) ((JsProxy*)x)->js
#define JsProxy_VAL(x) hiwire_get(JsProxy_REF(x))
#define JsProxy_DICT(x) (((JsProxy*)x)->dict)


static int
JsProxy_clear(PyObject* self)
{
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

// clang-format off
static PyNumberMethods JsProxy_NumberMethods = {
  .nb_bool = JsProxy_Bool
};
// clang-format on

static PyTypeObject JsProxyType = {
  .tp_name = "pyodide.ffi.JsProxy",
  .tp_basicsize = sizeof(JsProxy),
  .tp_dealloc = (destructor)JsProxy_dealloc,
  .tp_getattro = JsProxy_GetAttr,
  .tp_setattro = JsProxy_SetAttr,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "A proxy to make a Javascript object behave like a Python object",
  .tp_as_number = &JsProxy_NumberMethods,
  .tp_repr = JsProxy_Repr,
  .tp_dictoffset = offsetof(JsProxy, dict),
};

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

PyObject*
JsProxy_create(JsVal object)
{
  PyTypeObject* type = &JsProxyType;

  bool success = false;
  PyObject* result = NULL;

  result = type->tp_alloc(type, 0);
  FAIL_IF_NONZERO(JsProxy_cinit(result, object));

  success = true;
finally:
  if (!success) {
    Py_CLEAR(result);
  }
  return result;
}


int
jsproxy_init(PyObject* core_module)
{
  bool success = false;
  FAIL_IF_MINUS_ONE(PyType_Ready(&JsProxyType));

  success = true;
finally:
  return success ? 0 : -1;
}
