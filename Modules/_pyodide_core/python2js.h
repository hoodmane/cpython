#ifndef PYTHON2JS_H
#define PYTHON2JS_H

// clang-format off
#include "Python.h"
// clang-format on
#include "jslib.h"

/**
 * Do a shallow conversion from python to JavaScript. Convert immutable types
 * with equivalent JavaScript immutable types, but all other types are proxied.
 */
JsVal
python2js(PyObject* x);

#endif /* PYTHON2JS_H */
