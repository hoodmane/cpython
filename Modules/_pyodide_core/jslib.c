#include "jslib.h"
#include "error_handling.h"


EM_JS_NUM(int, Jsv_type, (JsVal val, char* buf, int size), {
  return stringToUTF8(val?.constructor?.name ?? "unknown", buf, size);
});
