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

