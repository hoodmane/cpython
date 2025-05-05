#include "jslib.h"
#include "error_handling.h"
#include "jsmemops.h"


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



EM_JS(JsVal, _Jsv_get_true, (void), {
    return true;
})

EM_JS(JsVal, _Jsv_get_false, (void), {
    return false;
})

EM_JS(JsVal, _Jsv_get_undefined, (void), {
    return undefined;
})

