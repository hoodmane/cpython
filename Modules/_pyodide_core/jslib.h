#include <sys/types.h>
#include <hiwire.h>
#include <emscripten.h>

typedef __externref_t JsVal;
typedef HwRef JsRef;

#define JS_NULL __builtin_wasm_ref_null_extern()
int JsvNull_Check(JsVal);

// Special JsRefs for singleton constants.
extern const JsRef Jsr_undefined;
extern const JsRef Jsr_true;
extern const JsRef Jsr_false;

#define Jsv_undefined hiwire_get(Jsr_undefined)
#define Jsv_true hiwire_get(Jsr_true)
#define Jsv_false hiwire_get(Jsr_false)


#undef false
#undef true
// These work for both C and javascript.
// In C !!0 ==> 0 and in javascript !!0 ==> false
// In C !!1 ==> 1 and in javascript !!1 ==> true
// clang-format off
#define false (!!0)
#define true (!!1)

#define hiwire_CLEAR(x)                                                        \
  do {                                                                         \
    hiwire_decref(x);                                                          \
    x = NULL;                                                                  \
  } while (0)



int
Jsv_type(JsVal obj, char* buf, int size);

JsVal
JsvNum_fromInt(int x);

JsVal
JsvNum_fromDouble(double x);

JsVal
JsvNum_fromDigits(const unsigned int* digits, size_t ndigits);

bool
Jsv_equal(JsVal a, JsVal b);

bool
Jsv_not_equal(JsVal a, JsVal b);

// ==================== Conversions between JsRef and JsVal ====================

// Like hiwire_new except if the argument is JS_NULL it returns NULL instead of crashing.
// Upstream to hiwire?
JsRef
JsRef_new(JsVal v);

// Like hiwire_get except if the argument is NULL it returns JS_NULL instead of crashing.
// Upstream to hiwire?
JsVal
JsRef_toVal(JsRef ref);

// ==================== JsvObject API  ====================

JsVal
JsvObject_New(void);

JsVal
JsvObject_toString(JsVal obj);

int
JsvObject_SetAttr(JsVal obj, JsVal attr, JsVal value);

// ==================== JsvFunction API  ====================

bool
JsvFunction_Check(JsVal obj);

JsVal
JsvFunction_CallBound(JsVal func, JsVal this, JsVal args);


// ==================== JsvArray API  ====================

JsVal
JsvArray_New(void);

bool
JsvArray_Check(JsVal obj);

int
JsvArray_Push(JsVal obj, JsVal val);
