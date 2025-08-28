#ifndef JSLIB_H
#define JSLIB_H

#include <sys/types.h>
#include <hiwire.h>
#include <emscripten.h>
#include "Python.h"

typedef __externref_t JsVal;
typedef HwRef JsRef;

JsVal JsvError_Create(void);
int JsvError_Check(JsVal);

#define JS_ERROR JsvError_Create()

#define JsvNull_Check(v) __builtin_wasm_ref_is_null_extern(v)

// Special JsRefs for singleton constants.
extern const JsRef Jsr_undefined;
extern const JsRef Jsr_true;
extern const JsRef Jsr_false;

#define Jsv_undefined hiwire_get(Jsr_undefined)
#define Jsv_true hiwire_get(Jsr_true)
#define Jsv_false hiwire_get(Jsr_false)
#define Jsv_null __builtin_wasm_ref_null_extern()

#undef false
#undef true
// These work for both C and javascript.
// In C !!0 ==> 0 and in javascript !!0 ==> false
// In C !!1 ==> 1 and in javascript !!1 ==> true
// clang-format off
#define false (!!0)
#define true (!!1)

typedef struct Js_Identifier
{
  const char* string;
  JsRef object;
} Js_Identifier;

#define Js_static_string_init(value) { .string = value, .object = NULL }
#define Js_static_string(varname, value)                                       \
  static Js_Identifier varname = Js_static_string_init(value)
#define Js_IDENTIFIER(varname) Js_static_string(JsId_##varname, #varname)


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

bool
Jsv_equal(JsVal a, JsVal b);

bool
Jsv_not_equal(JsVal a, JsVal b);

bool
Jsv_to_bool(JsVal);

// ==================== Conversions between JsRef and JsVal ====================

// Like hiwire_new except if the argument is JS_ERROR it returns NULL instead of crashing.
// Upstream to hiwire?
JsRef
JsRef_new(JsVal v);

// Like hiwire_get except if the argument is NULL it returns JS_ERROR instead of crashing.
// Upstream to hiwire?
JsVal
JsRef_toVal(JsRef ref);

// ==================== Primitive Conversions ====================

JsVal
JsvUTF8ToString(const char*);

JsRef
JsrString_FromId(Js_Identifier* id);

JsVal
JsvString_FromId(Js_Identifier* id);

// ==================== JsvObject API  ====================

JsVal
JsvObject_New(void);

JsVal
JsvObject_toString(JsVal obj);

int
JsvObject_SetAttr(JsVal obj, JsVal attr, JsVal value);

JsVal
JsvObject_CallMethod_OneArg(JsVal obj, JsVal name, JsVal arg);


JsVal
JsvObject_CallMethodId_OneArg(JsVal obj, Js_Identifier* name_id, JsVal arg);


JsVal
JsvObject_CallMethodId_TwoArgs(JsVal obj, Js_Identifier* name_id, JsVal arg1, JsVal arg2);

// ==================== JsvFunction API  ====================

bool
JsvFunction_Check(JsVal obj);

JsVal
JsvFunction_CallBound(JsVal func, JsVal this, JsVal args);

JsVal
JsvFunction_Construct(JsVal func, JsVal args);

// ==================== JsvArray API  ====================

JsVal
JsvArray_New(void);

bool
JsvArray_Check(JsVal obj);

int
JsvArray_Push(JsVal obj, JsVal val);


JsVal
JsvArray_Get(JsVal, int);

int
JsvArray_Set(JsVal, int, JsVal);

JsVal
JsvArray_Delete(JsVal, int);

void JsvArray_Extend(JsVal, JsVal);

int
JsvArray_Insert(JsVal arr, int idx, JsVal value);

JsVal
JsvArray_ShallowCopy(JsVal obj);


JsVal
JsvArray_slice(JsVal obj, int length, int start, int stop, int step);

int
JsvArray_slice_assign(JsVal idobj,
                      int slicelength,
                      int start,
                      int stop,
                      int step,
                      int values_length,
                      PyObject** values);

void __attribute__((__noreturn__))
JsvError_Throw(JsVal e);

#endif // JSLIB_H
