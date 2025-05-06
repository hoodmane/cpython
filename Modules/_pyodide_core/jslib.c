#include "jslib.h"
#include "error_handling.h"
#include "jsmemops.h"

#undef true
#undef false

#define JS_BUILTIN(val) JS_CONST(val, val)
#define JS_INIT_CONSTS()                                                       \
  JS_BUILTIN(undefined)                                                        \
  JS_BUILTIN(true)                                                             \
  JS_BUILTIN(false)

// we use HIWIRE_INIT_CONSTS once in C and once inside JS with different
// definitions of HIWIRE_INIT_CONST to ensure everything lines up properly
// C definition:
#define JS_CONST(name, value) EMSCRIPTEN_KEEPALIVE const JsRef Jsr_##name;
JS_INIT_CONSTS();

#undef JS_CONST

#define JS_CONST(name, value) HEAP32[_Jsr_##name / 4] = _hiwire_intern(value);

EM_JS_MACROS(void, jslib_init_js, (void), {
  JS_INIT_CONSTS();
});


__attribute__((constructor)) void
jslib_init(void)
{
  jslib_init_js();
}

EM_JS_NUM(int, Jsv_type, (JsVal val, char* buf, int size), {
  return stringToUTF8(val?.constructor?.name ?? "unknown", buf, size);
});

EM_JS(JsVal, JsvNum_fromInt, (int x), {
  return x;
})

EM_JS(JsVal, JsvNum_fromDouble, (double val), {
  return val;
});


EM_JS_MACROS(JsVal,
JsvNum_fromDigits,
(const unsigned int* digits, size_t ndigits),
{
  let result = BigInt(0);
  for (let i = 0; i < ndigits; i++) {
    result += BigInt(DEREF_U32(digits, i)) << BigInt(32 * i);
  }
  result += BigInt(DEREF_U32(digits, ndigits - 1) & 0x80000000)
            << BigInt(1 + 32 * (ndigits - 1));
  if (-Number.MAX_SAFE_INTEGER < result &&
      result < Number.MAX_SAFE_INTEGER) {
    result = Number(result);
  }
  return result;
});


EM_JS_BOOL(bool, Jsv_equal, (JsVal a, JsVal b), { return !!(a === b); });
EM_JS_BOOL(bool, Jsv_not_equal, (JsVal a, JsVal b), { return !!(a !== b); });


// ==================== Conversions between JsRef and JsVal ====================

JsRef
JsRef_new(JsVal v)
{
  if (JsvNull_Check(v)) {
    return NULL;
  }
  return hiwire_new(v);
}

JsVal
JsRef_toVal(JsRef ref)
{
  if (ref == NULL) {
    return JS_NULL;
  }
  return hiwire_get(ref);
}

// ==================== JsvObject API  ====================

EM_JS(JsVal, JsvObject_New, (void), {
  return {};
});

EM_JS_NUM(int, JsvObject_SetAttr, (JsVal obj, JsVal attr, JsVal value), {
  obj[attr] = value;
});


EM_JS_VAL(JsVal,
JsvObject_toString, (JsVal obj), {
  if (hasMethod(obj, "toString")) {
    return obj.toString();
  }
  return Object.prototype.toString.call(obj);
});


// ==================== JsvFunction API  ====================

EM_JS_BOOL(bool, JsvFunction_Check, (JsVal obj), {
  // clang-format off
  return typeof obj === 'function';
  // clang-format on
});

EM_JS_VAL(JsVal, JsvFunction_CallBound, (JsVal func, JsVal this_, JsVal args), {
  return nullToUndefined(Function.prototype.apply.apply(func, [ this_, args ]));
});

// clang-format off
EM_JS_VAL(JsVal,
JsvFunction_Construct,
(JsVal func, JsVal args),
{
  return nullToUndefined(Reflect.construct(func, args));
});
// clang-format on


// ==================== JsvArray API  ====================

EM_JS(JsVal, JsvArray_New, (void), {
  return [];
});

EM_JS_BOOL(bool, JsvArray_Check, (JsVal obj), {
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

EM_JS(int, JsvArray_Push, (JsVal arr, JsVal obj), {
  return arr.push(obj);
});

EM_JS(void, jslib_toplevel_helpers, (void), {}
const nullToUndefined = (x) => (x === null ? undefined : x);
const getTypeTag = (x) => {
  try {
    return Object.prototype.toString.call(x);
  } catch (e) {
    return "";
  }
};

/**
 * Observe whether a method exists or not
 *
 * Invokes getters but catches any error produced by a getter and throws it away.
 * Never throws an error
 *
 * obj: an object
 * prop: a string or symbol
 */
const hasMethod = (obj, prop) => {
  try {
    return typeof obj[prop] === "function";
  } catch (e) {
    return false;
  }
};
)
