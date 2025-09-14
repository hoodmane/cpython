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

/**
 * Like python2js except in the handling of PyProxy creation.
 *
 * If proxies is NULL, will throw an error instead of creating a PyProxy.
 * Otherwise, proxies should be an Array and python2js_track_proxies will add
 * the proxy to the array if one is created.
 */
JsVal
python2js_track_proxies(PyObject* x, JsVal proxies, bool gc_register);

/**
 * dict_converter should be a JavaScript function that converts an Iterable of
 * pairs into the desired JavaScript object. If dict_converter is NULL, we use
 * python2js_with_depth which converts dicts to Map (the default)
 */
JsVal
python2js_custom(PyObject* x,
                 int depth,
                 JsVal proxies,
                 JsVal dict_converter,
                 JsVal default_converter,
                 JsVal eager_converter);


int
python2js_init(PyObject* core);

extern PyObject* py_jsnull;

#endif /* PYTHON2JS_H */
