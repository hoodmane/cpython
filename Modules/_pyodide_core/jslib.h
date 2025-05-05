#include <sys/types.h>

typedef __externref_t JsVal;

#define JS_NULL __builtin_wasm_ref_null_extern()
int JsvNull_Check(JsVal);

// TODO: Replace these with constants
#define Jsv_true _Jsv_get_true()
#define Jsv_false _Jsv_get_false()
#define Jsv_undefined _Jsv_get_undefined()


#undef false
#undef true
// These work for both C and javascript.
// In C !!0 ==> 0 and in javascript !!0 ==> false
// In C !!1 ==> 1 and in javascript !!1 ==> true
// clang-format off
#define false (!!0)
#define true (!!1)


int
Jsv_type(JsVal obj, char* buf, int size);

JsVal
JsvNum_fromInt(int x);

JsVal
JsvNum_fromDouble(double x);

JsVal
JsvNum_fromDigits(const unsigned int* digits, size_t ndigits);


JsVal
_Jsv_get_true(void);

JsVal
_Jsv_get_false(void);

JsVal
_Jsv_get_undefined(void);
