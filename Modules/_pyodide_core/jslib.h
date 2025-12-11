#ifndef JSLIB_H
#define JSLIB_H

#include <stdbool.h>
#include <sys/types.h>
#include <hiwire.h>
#include <emscripten.h>
#include "Python.h"

typedef __externref_t JsVal;
typedef HwRef JsRef;

JsVal _PyJsvError_Create(void);
int _PyJsvError_Check(JsVal);

#define JS_ERROR hiwire_get(_PyJsr_error)
#define JS_NULL __builtin_wasm_ref_null_extern()

#define JsvNull_Check(v) __builtin_wasm_ref_is_null_extern(v)

int
_PyJsvNoValue_Check(JsVal);

// Special JsRefs for singleton constants.
extern JsRef _PyJsr_undefined;
extern JsRef _PyJsr_true;
extern JsRef _PyJsr_false;
extern JsRef _PyJsr_error;
extern JsRef _PyJsr_novalue;

#define Jsv_undefined hiwire_get(_PyJsr_undefined)
#define Jsv_true hiwire_get(_PyJsr_true)
#define Jsv_false hiwire_get(_PyJsr_false)
#define Jsv_null __builtin_wasm_ref_null_extern()
#define Jsv_novalue hiwire_get(_PyJsr_novalue)

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
_PyJsv_type(JsVal obj, char* buf, int size);

JsVal
_PyJsvNum_fromInt(int x);

JsVal
_PyJsvNum_fromDouble(double x);

JsVal
_PyJsv_BigIntToNum(JsVal x);

bool
_PyJsv_equal(JsVal a, JsVal b);

bool
_PyJsv_not_equal(JsVal a, JsVal b);

bool
_PyJsv_to_bool(JsVal);

// ==================== Conversions between JsRef and JsVal ====================

// Like hiwire_new except if the argument is JS_ERROR it returns NULL instead of crashing.
// Upstream to hiwire?
JsRef
_PyJsRef_new(JsVal v);

// Like hiwire_get except if the argument is NULL it returns JS_ERROR instead of crashing.
// Upstream to hiwire?
JsVal
_PyJsRef_toVal(JsRef ref);

// ==================== Primitive Conversions ====================

JsVal
_PyJsvUTF8ToString(const char*);

JsRef
_PyJsrString_FromId(Js_Identifier* id);

JsVal
_PyJsvString_FromId(Js_Identifier* id);

// ==================== JsvObject API  ====================

JsVal
_PyJsvObject_New(void);

JsVal
_PyJsvObject_toString(JsVal obj);

int
_PyJsvObject_SetAttr(JsVal obj, JsVal attr, JsVal value);

JsVal
_PyJsvObject_CallMethod_OneArg(JsVal obj, JsVal name, JsVal arg);

JsVal
_PyJsvObject_CallMethodId_NoArgs(JsVal obj, Js_Identifier* name_id);

JsVal
_PyJsvObject_CallMethodId_OneArg(JsVal obj, Js_Identifier* name_id, JsVal arg);


JsVal
_PyJsvObject_CallMethodId_TwoArgs(JsVal obj, Js_Identifier* name_id, JsVal arg1, JsVal arg2);

// ==================== JsvFunction API  ====================

bool
_PyJsvFunction_Check(JsVal obj);

bool
_PyJsvGenerator_Check(JsVal obj);

JsVal
_PyJsvFunction_CallBound(JsVal func, JsVal this, JsVal args);

JsVal
_PyJsvFunction_Construct(JsVal func, JsVal args);

// ==================== JsvArray API  ====================

JsVal
_PyJsvArray_New(void);

bool
_PyJsvArray_Check(JsVal obj);

int
_PyJsvArray_Push(JsVal obj, JsVal val);


JsVal
_PyJsvArray_Get(JsVal, int);

int
_PyJsvArray_Set(JsVal, int, JsVal);

JsVal
_PyJsvArray_Delete(JsVal, int);

void 
_PyJsvArray_Extend(JsVal, JsVal);

int
_PyJsvArray_Insert(JsVal arr, int idx, JsVal value);

JsVal
_PyJsvArray_ShallowCopy(JsVal obj);


JsVal
_PyJsvArray_Slice(JsVal obj, int length, int start, int stop, int step);

int
_PyJsvArray_SliceAssign(JsVal idobj,
                      int slicelength,
                      int start,
                      int stop,
                      int step,
                      int values_length,
                      PyObject** values);

void __attribute__((__noreturn__))
_PyJsvError_Throw(JsVal e);

JsVal
_PyJsvLiteralMap_New(void);

JsVal
_PyJsvMap_New(void);

int
_PyJsvMap_Set(JsVal map, JsVal key, JsVal val);

/**
 * Create a new Set.
 */
JsVal
_PyJsvSet_New(void);

/**
 * Does set.add(key).
 */
int
_PyJsvSet_Add(JsVal mapid, JsVal keyid);

#endif // JSLIB_H
