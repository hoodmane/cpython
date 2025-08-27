#include "jslib.h"
#include "python2js.h"
#include "Python.h"
#include "error_handling.h"
#include "jsmemops.h"
#include "jsproxy.h"
#include "pyproxy.h"

///////////////////////////////////////////////////////////////////////////////
//
// Simple Converters
//
// These convert float, int, and unicode types. Used by python2js_immutable
// (which also handles bool and None).

static JsVal
_python2js_float(PyObject* x)
{
  double x_double = PyFloat_AsDouble(x);
  if (x_double == -1.0 && PyErr_Occurred()) {
    return JS_ERROR;
  }
  return JsvNum_fromDouble(x_double);
}

#if PYLONG_BITS_IN_DIGIT == 15
#error "Expected PYLONG_BITS_IN_DIGIT == 30"
#endif

EM_JS(JsVal, _python2js_long_js_small, (int64_t x), {
  if (-Number.MAX_SAFE_INTEGER < x && x < Number.MAX_SAFE_INTEGER) {
    return Number(x);
  }
  return x;
})

EM_JS_MACROS(JsVal,
_python2js_long_js_big, (const unsigned int *digits, size_t ndigits, uint8_t negative,
                         uint8_t bits_per_digit, uint8_t digit_size),
{
  let result = BigInt(0);
  if (digit_size === 16) {
    for (let i = 0; i < ndigits; i++) {
      result += BigInt(DEREF_U16(digits, i))
        << BigInt(bits_per_digit * i);
    }
  } else {
    for (let i = 0; i < ndigits; i++) {
      result += BigInt(DEREF_U32(digits, i))
        << BigInt(bits_per_digit * i);
    }
  }
  if (negative) {
    result *= -1n;
  }
  if (-Number.MAX_SAFE_INTEGER < result &&
    result < Number.MAX_SAFE_INTEGER) {
    result = Number(result);
  }
  return result;
});

static JsVal
_python2js_long(PyObject* x)
{
  PyLongExport export_long;
  if (PyLong_Export(x, &export_long) == -1) {
    return JS_ERROR;
  }
  JsVal result;
  if (export_long.digits == NULL) {
    result = _python2js_long_js_small(export_long.value);
  } else {
    const PyLongLayout *layout = PyLong_GetNativeLayout();
    result = _python2js_long_js_big(export_long.digits, export_long.ndigits,
                                    export_long.negative,
                                    layout->bits_per_digit, layout->digit_size);
  }

  PyLong_FreeExport(&export_long);
  return result;
}

// python2js string conversion
//
// FAQs:
//
// Q: Why do we use this approach rather than TextDecoder?
//
// A: TextDecoder does have an 'ascii' encoding and a 'ucs2' encoding which
// sound promising. They work in many cases but not in all cases, particularly
// when strings contain weird unprintable bytes. I suspect these conversion
// functions are also considerably faster than TextDecoder because it takes
// complicated extra code to cause the problematic edge case behavior of
// TextDecoder.
//
//
// Q: Is it okay to use str += more_str in a loop? Does this perform a lot of
// copies?
//
// A: We haven't profiled this but I suspect that the JS VM understands this
// code quite well and can git it into very performant code.
// TODO: someone should compare += in a loop to building a list and using
// list.join("") and see if one is faster than the other.

EM_JS_VAL(JsVal, _python2js_ucs1, (const char* ptr, int len), {
  let jsstr = "";
  for (let i = 0; i < len; ++i) {
    jsstr += String.fromCharCode(DEREF_U8(ptr, i));
  }
  return jsstr;
});

EM_JS_VAL(JsVal, _python2js_ucs2, (const char* ptr, int len), {
  let jsstr = "";
  for (let i = 0; i < len; ++i) {
    jsstr += String.fromCharCode(DEREF_U16(ptr, i));
  }
  return jsstr;
});

EM_JS_VAL(JsVal, _python2js_ucs4, (const char* ptr, int len), {
  let jsstr = "";
  for (let i = 0; i < len; ++i) {
    jsstr += String.fromCodePoint(DEREF_U32(ptr, i));
  }
  return jsstr;
});

static JsVal
_python2js_unicode(PyObject* x)
{
  int kind = PyUnicode_KIND(x);
  char* data = (char*)PyUnicode_DATA(x);
  int length = (int)PyUnicode_GET_LENGTH(x);
  switch (kind) {
    case PyUnicode_1BYTE_KIND:
      return _python2js_ucs1(data, length);
    case PyUnicode_2BYTE_KIND:
      return _python2js_ucs2(data, length);
    case PyUnicode_4BYTE_KIND:
      return _python2js_ucs4(data, length);
  }
  PyErr_SetString(PyExc_SystemError, "Invalid unicode object");
  return JS_ERROR;
}

JsVal
python2js(PyObject* x)
{
  if (Py_IsNone(x)) {
    return Jsv_undefined;
  } else if (Py_IsTrue(x)) {
    return Jsv_true;
  } else if (Py_IsFalse(x)) {
    return Jsv_false;
  } else if (x == py_jsnull) {
    return Jsv_null;
  } else if (PyLong_Check(x)) {
    return _python2js_long(x);
  } else if (PyFloat_Check(x)) {
    return _python2js_float(x);
  } else if (PyUnicode_Check(x)) {
    return _python2js_unicode(x);
  } else if (JsProxy_Check(x)) {
    return JsProxy_Val(x);
  }
  return pyproxy_new(x);
}
