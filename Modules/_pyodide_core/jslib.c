#include "jslib.h"
#include "error_handling.h"
#include "jsmemops.h"

#ifdef DEBUG_F
bool tracerefs = false;
#endif

#undef true
#undef false

#define JS_BUILTIN(val) JS_CONST(val, val)
#define JS_INIT_CONSTS()                                                       \
  JS_BUILTIN(undefined)                                                        \
  JS_BUILTIN(true)                                                             \
  JS_BUILTIN(false)                                                            \
  JS_CONST(error, _PyJsvError_Create())                                           \
  JS_CONST(novalue, { noValueMarker : 1 })

// we use HIWIRE_INIT_CONSTS once in C and once inside JS with different
// definitions of HIWIRE_INIT_CONST to ensure everything lines up properly
// C definition:
#define JS_CONST(name, value) EMSCRIPTEN_KEEPALIVE JsRef _PyJsr_##name;
JS_INIT_CONSTS();

#undef JS_CONST

#define JS_CONST(name, value) HEAP32[__PyJsr_##name / 4] = _hiwire_intern(value);

EM_JS_MACROS(void, _Py_jslib_init_js, (void), {
  JS_INIT_CONSTS();
  Module.novalue = _hiwire_get(HEAP32[__PyJsr_novalue / 4]);
  Module.error = _hiwire_get(HEAP32[__PyJsr_error / 4]);
});


__attribute__((constructor)) void
_Py_jslib_init(void)
{
  _Py_jslib_init_js();
}

EM_JS(int, _PyJsvNoValue_Check, (JsVal v), {
  return v === Module.novalue;
});

EM_JS_NUM(int, _PyJsv_type, (JsVal val, char* buf, int size), {
  return stringToUTF8(val?.constructor?.name ?? "unknown", buf, size);
});

EM_JS(JsVal, _PyJsvNum_fromInt, (int x), {
  return x;
})

EM_JS(JsVal, _PyJsvNum_fromDouble, (double val), {
  return val;
});

EM_JS_BOOL(bool, _PyJsv_equal, (JsVal a, JsVal b), { return !!(a === b); });
EM_JS_BOOL(bool, _PyJsv_not_equal, (JsVal a, JsVal b), { return !!(a !== b); });

EM_JS(bool, _PyJsv_to_bool, (JsVal x), {
  return !!x;
})

// ==================== Conversions between JsRef and JsVal ====================

JsRef
_PyJsRef_new(JsVal v)
{
  if (JsvNull_Check(v)) {
    return NULL;
  }
  return hiwire_new(v);
}

JsVal
_PyJsRef_toVal(JsRef ref)
{
  if (ref == NULL) {
    return JS_ERROR;
  }
  return hiwire_get(ref);
}

// ==================== Primitive Conversions ====================

EM_JS(JsVal, _PyJsvUTF8ToString, (const char* ptr), {
  return UTF8ToString(ptr);
})

EMSCRIPTEN_KEEPALIVE JsRef
_PyJsrString_FromId(Js_Identifier* id)
{
  if (!id->object) {
    id->object = hiwire_intern(_PyJsvUTF8ToString(id->string));
  }
  return id->object;
}

EMSCRIPTEN_KEEPALIVE JsVal
_PyJsvString_FromId(Js_Identifier* id)
{
  return _PyJsRef_toVal(_PyJsrString_FromId(id));
}

// ==================== JsvObject API  ====================

EM_JS(JsVal, _PyJsvObject_New, (void), {
  return {};
});

EM_JS_NUM(int, _PyJsvObject_SetAttr, (JsVal obj, JsVal attr, JsVal value), {
  obj[attr] = value;
});


EM_JS_VAL(JsVal,
_PyJsvObject_toString, (JsVal obj), {
  if (hasMethod(obj, "toString")) {
    return obj.toString();
  }
  return Object.prototype.toString.call(obj);
});

EM_JS_VAL(JsVal, _PyJsvObject_CallMethod_NoArgs, (JsVal obj, JsVal meth), {
  return obj[meth]();
})

EM_JS_VAL(JsVal, _PyJsvObject_CallMethod_OneArg, (JsVal obj, JsVal meth, JsVal arg), {
  return obj[meth](arg);
})

EM_JS_VAL(JsVal, _PyJsvObject_CallMethod_TwoArgs, (JsVal obj, JsVal meth, JsVal arg1, JsVal arg2), {
  return obj[meth](arg1, arg2);
})

JsVal
_PyJsvObject_CallMethodId_NoArgs(JsVal obj, Js_Identifier* name_id)
{
  return _PyJsvObject_CallMethod_NoArgs(obj, _PyJsvString_FromId(name_id));
}

JsVal
_PyJsvObject_CallMethodId_OneArg(JsVal obj, Js_Identifier* name_id, JsVal arg)
{
  return _PyJsvObject_CallMethod_OneArg(obj, _PyJsvString_FromId(name_id), arg);
}


JsVal
_PyJsvObject_CallMethodId_TwoArgs(JsVal obj,
                               Js_Identifier* name_id,
                               JsVal arg1,
                               JsVal arg2)
{
  return _PyJsvObject_CallMethod_TwoArgs(obj, _PyJsvString_FromId(name_id), arg1, arg2);
}

// ==================== JsvFunction API  ====================

EM_JS_BOOL(bool, _PyJsvFunction_Check, (JsVal obj), {
  // clang-format off
  return typeof obj === 'function';
  // clang-format on
});

EM_JS_VAL(JsVal, _PyJsvFunction_CallBound, (JsVal func, JsVal this_, JsVal args), {
  return Function.prototype.apply.apply(func, [ this_, args ]);
});

// clang-format off
EM_JS_VAL(JsVal,
_PyJsvFunction_Construct,
(JsVal func, JsVal args),
{
  return Reflect.construct(func, args);
});
// clang-format on

EM_JS_BOOL(bool, _PyJsvGenerator_Check, (JsVal obj), {
  return getTypeTag(obj) === "[object Generator]";
});

// ==================== JsvArray API  ====================

EM_JS(JsVal, _PyJsvArray_New, (void), {
  return [];
});

EM_JS_BOOL(bool, _PyJsvArray_Check, (JsVal obj), {
  if (Array.isArray(obj)) {
    return true;
  }
  const typeTag = getTypeTag(obj);
  // We want to treat some standard array-like objects as Array.
  // clang-format off
  if (typeTag === "[object HTMLCollection]" || typeTag === "[object NodeList]") {
    // clang-format on
    return true;
  }
  // What if it's a TypedArray?
  // clang-format off
  if (ArrayBuffer.isView(obj) && obj.constructor.name !== "DataView") {
    // clang-format on
    return true;
  }
  return false;
});

EM_JS(int, _PyJsvArray_Push, (JsVal arr, JsVal obj), {
  return arr.push(obj);
});

EM_JS_VAL(JsVal, _PyJsvArray_Get, (JsVal arr, int idx), {
  const result = arr[idx];
  // clang-format off
  if (result === undefined && !(idx in arr)) {
    // clang-format on
    return Module.error;
  }
  return result;
});

EM_JS_NUM(int, _PyJsvArray_Set, (JsVal arr, int idx, JsVal val), {
  arr[idx] = val;
});

EM_JS_VAL(JsVal, _PyJsvArray_Delete, (JsVal arr, int idx), {
  // Weird edge case: allow deleting an empty entry, but we raise a key error if
  // access is attempted.
  if (idx < 0 || idx >= arr.length) {
    return Module.error;
  }
  return arr.splice(idx, 1)[0];
});

EM_JS(void, _PyJsvArray_Extend, (JsVal arr, JsVal vals), {
  arr.push(...vals);
});
// clang-format on

EM_JS_NUM(int, _PyJsvArray_Insert, (JsVal arr, int idx, JsVal value), {
  arr.splice(idx, 0, value);
});

EM_JS_NUM(JsVal, _PyJsvArray_ShallowCopy, (JsVal arr), {
  return ("slice" in arr) ? arr.slice() : Array.from(arr);
})

EM_JS_VAL(JsVal,
_PyJsvArray_Slice,
(JsVal obj, int length, int start, int stop, int step),
{
  let result;
  if (step === 1) {
    result = obj.slice(start, stop);
  } else {
    result = Array.from({ length }, (_, i) => obj[start + i * step]);
  }
  return result;
});

EM_JS_NUM(int,
_PyJsvArray_SliceAssign,
(JsVal obj, int slicelength, int start, int stop, int step, int values_length, PyObject **values),
{
  let jsvalues = [];
  for (let i = 0; i < values_length; i++) {
    const ref = __Py_python2js(DEREF_U32(values, i));
    if (ref === Module.error){
      return -1;
    }
    jsvalues.push(ref);
  }
  if (step === 1) {
    obj.splice(start, slicelength, ...jsvalues);
  } else {
    if (values !== 0) {
      for (let i = 0; i < slicelength; i++) {
        obj.splice(start + i * step, 1, jsvalues[i]);
      }
    } else {
      for(let i = slicelength - 1; i >= 0; i --){
        obj.splice(start + i * step, 1);
      }
    }
  }
});


EM_JS(void __attribute__((__noreturn__)), _PyJsvError_Throw, (JsVal e), { throw e; })

// ==================== Js Map API  ====================

EM_JS_VAL(JsVal, _PyJsvMap_New, (void), {
  return new Map();
})

EM_JS_VAL(JsVal, _PyJsvLiteralMap_New, (void), {
  return new API.LiteralMap();
})

EM_JS_NUM(int, _PyJsvMap_Set, (JsVal map, JsVal key, JsVal val), {
  map.set(key, val);
})

// ==================== JsSet API  ====================

EM_JS_VAL(JsVal, _PyJsvSet_New, (void), {
  return new Set();
})

EM_JS_NUM(int, _PyJsvSet_Add, (JsVal set, JsVal val), {
  set.add(val);
})
