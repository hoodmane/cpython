typedef __externref_t JsVal;

#define JS_NULL __builtin_wasm_ref_null_extern()
int JsvNull_Check(JsVal);

#undef false
#undef true
// These work for both C and javascript.
// In C !!0 ==> 0 and in javascript !!0 ==> false
// In C !!1 ==> 1 and in javascript !!1 ==> true
// clang-format off
#define false (!!0)
#define true (!!1)
