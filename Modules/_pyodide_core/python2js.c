#include "jslib.h"
#include "Python.h"
#include "error_handling.h"
#include "jsmemops.h"
#include "jsproxy.h"

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
    return JS_NULL;
  }
  return JsvNum_fromDouble(x_double);
}

#if PYLONG_BITS_IN_DIGIT == 15
#error "Expected PYLONG_BITS_IN_DIGIT == 30"
#endif

static JsVal
_python2js_long(PyObject* x)
{
  int overflow;
  long x_long = PyLong_AsLongAndOverflow(x, &overflow);
  if (x_long == -1) {
    if (!overflow) {
      FAIL_IF_ERR_OCCURRED();
    } else {
      // We want to group into u32 chunks for convenience of
      // JsvNum_fromDigits. If the number of bits is evenly divisible by
      // 32, we overestimate the number of needed u32s by one.
      size_t nbits = _PyLong_NumBits(x);
      size_t ndigits = (nbits >> 5) + 1;
      unsigned int digits[ndigits];
      FAIL_IF_MINUS_ONE(_PyLong_AsByteArray((PyLongObject*)x,
                                            (unsigned char*)digits,
                                            4 * ndigits,
                                            true /* little endian */,
                                            true /* signed */,
                                            true /* with_exceptions */));
      return JsvNum_fromDigits(digits, ndigits);
    }
  }
  return JsvNum_fromInt(x_long);
finally:
  return JS_NULL;
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
    default:
      assert(false /* invalid Unicode kind */);
  }
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
  } else if (PyLong_Check(x)) {
    return _python2js_long(x);
  } else if (PyFloat_Check(x)) {
    return _python2js_float(x);
  } else if (PyUnicode_Check(x)) {
    return _python2js_unicode(x);
  } else if (JsProxy_Check(x)) {
    return JsProxy_Val(x);
  }
  PyErr_Format(PyExc_TypeError, "No JavaScript conversion known for Python object: %R.", x);
  return JS_NULL;
}
