/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_abstract.h"      // _PyNumber_Index()
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()
#include "pycore_unicodeobject.h" // _PyUnicode_AsUTF8NoNUL()

PyDoc_STRVAR(_pyodide_core_JsProxy___dir____doc__,
"__dir__($self, /)\n"
"--\n"
"\n"
"Implementation for dir(proxy).\n"
"\n"
"Walk the prototype chain of the object and adds the ownPropertyNames\n"
"of each prototype.");

#define _PYODIDE_CORE_JSPROXY___DIR___METHODDEF    \
    {"__dir__", (PyCFunction)_pyodide_core_JsProxy___dir__, METH_NOARGS, _pyodide_core_JsProxy___dir____doc__},

static PyObject *
_pyodide_core_JsProxy___dir___impl(PyObject *self);

static PyObject *
_pyodide_core_JsProxy___dir__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsProxy___dir___impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsProxy_to_py__doc__,
"to_py($self, /, *, depth=-1, default_converter=None)\n"
"--\n"
"\n"
"Convert the JsProxy to a native Python object.\n"
"\n"
"  depth\n"
"    Limit the depth of the conversion. If a shallow conversion is\n"
"    desired, set ``depth`` to 1.\n"
"  default_converter\n"
"    If present, this will be invoked whenever Pyodide does not have some\n"
"    built in conversion for the object. If ``default_converter`` raises\n"
"    an error, the error will be allowed to propagate. Otherwise, the\n"
"    object returned will be used as the conversion.\n"
"    ``default_converter`` takes three arguments. The first argument is\n"
"    the value to be converted.");

#define _PYODIDE_CORE_JSPROXY_TO_PY_METHODDEF    \
    {"to_py", _PyCFunction_CAST(_pyodide_core_JsProxy_to_py), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsProxy_to_py__doc__},

static PyObject *
_pyodide_core_JsProxy_to_py_impl(PyObject *self, int depth,
                                 PyObject *default_converter);

static PyObject *
_pyodide_core_JsProxy_to_py(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 2
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(depth), &_Py_ID(default_converter), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"depth", "default_converter", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "to_py",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    int depth = -1;
    PyObject *default_converter = Py_None;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 0, /*maxpos*/ 0, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[0]) {
        depth = PyLong_AsInt(args[0]);
        if (depth == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    default_converter = args[1];
skip_optional_kwonly:
    return_value = _pyodide_core_JsProxy_to_py_impl(self, depth, default_converter);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsGenerator_send__doc__,
"send($self, arg, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSGENERATOR_SEND_METHODDEF    \
    {"send", (PyCFunction)_pyodide_core_JsGenerator_send, METH_O, _pyodide_core_JsGenerator_send__doc__},

PyDoc_STRVAR(_pyodide_core_JsGenerator_throw__doc__,
"throw($self, value, val=<unrepresentable>, tb=<unrepresentable>, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSGENERATOR_THROW_METHODDEF    \
    {"throw", _PyCFunction_CAST(_pyodide_core_JsGenerator_throw), METH_FASTCALL, _pyodide_core_JsGenerator_throw__doc__},

static PyObject *
_pyodide_core_JsGenerator_throw_impl(PyObject *self, PyObject *value,
                                     PyObject *val, PyObject *tb);

static PyObject *
_pyodide_core_JsGenerator_throw(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *value;
    PyObject *val = NULL;
    PyObject *tb = NULL;

    if (!_PyArg_CheckPositional("throw", nargs, 1, 3)) {
        goto exit;
    }
    value = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    val = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    tb = args[2];
skip_optional:
    return_value = _pyodide_core_JsGenerator_throw_impl(self, value, val, tb);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsGenerator_close__doc__,
"close($self, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSGENERATOR_CLOSE_METHODDEF    \
    {"close", (PyCFunction)_pyodide_core_JsGenerator_close, METH_NOARGS, _pyodide_core_JsGenerator_close__doc__},

static PyObject *
_pyodide_core_JsGenerator_close_impl(PyObject *self);

static PyObject *
_pyodide_core_JsGenerator_close(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsGenerator_close_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsAsyncGenerator_asend__doc__,
"asend($self, arg, /)\n"
"--\n"
"\n"
"Resume execution and \"send\" a value into the async generator.\n"
"\n"
"The arg argument becomes the result of the current yield expression.\n"
"The awaitable returned by the asend() method will return the next value\n"
"yielded by the generator or raises StopAsyncIteration if the\n"
"asynchronous generator returns. If the generator returned a value, this\n"
"value is discarded (because in Python async generators cannot return a\n"
"value).");

#define _PYODIDE_CORE_JSASYNCGENERATOR_ASEND_METHODDEF    \
    {"asend", (PyCFunction)_pyodide_core_JsAsyncGenerator_asend, METH_O, _pyodide_core_JsAsyncGenerator_asend__doc__},

PyDoc_STRVAR(_pyodide_core_JsAsyncGenerator_athrow__doc__,
"athrow($self, value, val=<unrepresentable>, tb=<unrepresentable>, /)\n"
"--\n"
"\n"
"Resume execution and raise an exception into the async generator.\n"
"\n"
"The exception is raised at the point where the generator was paused.");

#define _PYODIDE_CORE_JSASYNCGENERATOR_ATHROW_METHODDEF    \
    {"athrow", _PyCFunction_CAST(_pyodide_core_JsAsyncGenerator_athrow), METH_FASTCALL, _pyodide_core_JsAsyncGenerator_athrow__doc__},

static PyObject *
_pyodide_core_JsAsyncGenerator_athrow_impl(PyObject *self, PyObject *value,
                                           PyObject *val, PyObject *tb);

static PyObject *
_pyodide_core_JsAsyncGenerator_athrow(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *value;
    PyObject *val = NULL;
    PyObject *tb = NULL;

    if (!_PyArg_CheckPositional("athrow", nargs, 1, 3)) {
        goto exit;
    }
    value = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    val = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    tb = args[2];
skip_optional:
    return_value = _pyodide_core_JsAsyncGenerator_athrow_impl(self, value, val, tb);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsAsyncGenerator_aclose__doc__,
"aclose($self, /)\n"
"--\n"
"\n"
"Raise a GeneratorExit at the point where the generator was paused.\n"
"\n"
"If the generator function then exits gracefully, is already closed, or\n"
"raises GeneratorExit (by not catching the exception), then the returned\n"
"awaitable will raise a StopIteration exception. Any further awaitables\n"
"returned by subsequent calls to the asynchronous generator will raise a\n"
"StopAsyncIteration exception. If the generator yields a value, a\n"
"RuntimeError is raised. If the generator raises any other exception, it\n"
"is propagated to the caller. If the generator has already exited due to\n"
"an exception or normal exit, then further calls to aclose() will return\n"
"an awaitable that does nothing.");

#define _PYODIDE_CORE_JSASYNCGENERATOR_ACLOSE_METHODDEF    \
    {"aclose", (PyCFunction)_pyodide_core_JsAsyncGenerator_aclose, METH_NOARGS, _pyodide_core_JsAsyncGenerator_aclose__doc__},

static PyObject *
_pyodide_core_JsAsyncGenerator_aclose_impl(PyObject *self);

static PyObject *
_pyodide_core_JsAsyncGenerator_aclose(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsAsyncGenerator_aclose_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsProxy___enter____doc__,
"__enter__($self, /)\n"
"--\n"
"\n"
"If [Symbol.dispose] is present, implement context manager protocol.");

#define _PYODIDE_CORE_JSPROXY___ENTER___METHODDEF    \
    {"__enter__", (PyCFunction)_pyodide_core_JsProxy___enter__, METH_NOARGS, _pyodide_core_JsProxy___enter____doc__},

static PyObject *
_pyodide_core_JsProxy___enter___impl(PyObject *self);

static PyObject *
_pyodide_core_JsProxy___enter__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsProxy___enter___impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsProxy___exit____doc__,
"__exit__($self, exc_type=None, exc_value=None, exc_tb=None, /)\n"
"--\n"
"\n"
"If [Symbol.dispose] is present, implement context manager protocol.");

#define _PYODIDE_CORE_JSPROXY___EXIT___METHODDEF    \
    {"__exit__", _PyCFunction_CAST(_pyodide_core_JsProxy___exit__), METH_FASTCALL, _pyodide_core_JsProxy___exit____doc__},

static PyObject *
_pyodide_core_JsProxy___exit___impl(PyObject *self, PyObject *exc_type,
                                    PyObject *exc_value, PyObject *exc_tb);

static PyObject *
_pyodide_core_JsProxy___exit__(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *exc_type = Py_None;
    PyObject *exc_value = Py_None;
    PyObject *exc_tb = Py_None;

    if (!_PyArg_CheckPositional("__exit__", nargs, 0, 3)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    exc_type = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    exc_value = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    exc_tb = args[2];
skip_optional:
    return_value = _pyodide_core_JsProxy___exit___impl(self, exc_type, exc_value, exc_tb);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsPromise_then__doc__,
"then($self, /, onfulfilled=None, onrejected=None)\n"
"--\n"
"\n"
"The Promise.then API, wrapped to manage the handlers\' lifetimes.\n"
"\n"
"The references to the handlers are automatically released when the\n"
"promise resolves.");

#define _PYODIDE_CORE_JSPROMISE_THEN_METHODDEF    \
    {"then", _PyCFunction_CAST(_pyodide_core_JsPromise_then), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsPromise_then__doc__},

static PyObject *
_pyodide_core_JsPromise_then_impl(PyObject *self, PyObject *onfulfilled,
                                  PyObject *onrejected);

static PyObject *
_pyodide_core_JsPromise_then(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 2
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(onfulfilled), &_Py_ID(onrejected), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"onfulfilled", "onrejected", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "then",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    PyObject *onfulfilled = Py_None;
    PyObject *onrejected = Py_None;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 0, /*maxpos*/ 2, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[0]) {
        onfulfilled = args[0];
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    onrejected = args[1];
skip_optional_pos:
    return_value = _pyodide_core_JsPromise_then_impl(self, onfulfilled, onrejected);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsPromise_catch__doc__,
"catch($self, onrejected, /)\n"
"--\n"
"\n"
"The Promise.catch API, wrapped to manage the handler\'s lifetime.\n"
"\n"
"The reference to the handler is automatically released when the promise\n"
"resolves.");

#define _PYODIDE_CORE_JSPROMISE_CATCH_METHODDEF    \
    {"catch", (PyCFunction)_pyodide_core_JsPromise_catch, METH_O, _pyodide_core_JsPromise_catch__doc__},

PyDoc_STRVAR(_pyodide_core_JsPromise_finally___doc__,
"finally_($self, onfinally, /)\n"
"--\n"
"\n"
"The Promise.finally API, wrapped to manage the handler\'s lifetime.\n"
"\n"
"The reference to the handler is automatically released when the promise\n"
"resolves. Note the trailing underscore in the name; it is needed because\n"
"finally is a reserved keyword in Python.");

#define _PYODIDE_CORE_JSPROMISE_FINALLY__METHODDEF    \
    {"finally_", (PyCFunction)_pyodide_core_JsPromise_finally_, METH_O, _pyodide_core_JsPromise_finally___doc__},

PyDoc_STRVAR(_pyodide_core_JsProxy___aenter____doc__,
"__aenter__($self, /)\n"
"--\n"
"\n"
"Implement async context manager if [Symbol.asyncDispose] is present.");

#define _PYODIDE_CORE_JSPROXY___AENTER___METHODDEF    \
    {"__aenter__", (PyCFunction)_pyodide_core_JsProxy___aenter__, METH_NOARGS, _pyodide_core_JsProxy___aenter____doc__},

static PyObject *
_pyodide_core_JsProxy___aenter___impl(PyObject *self);

static PyObject *
_pyodide_core_JsProxy___aenter__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsProxy___aenter___impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsProxy___aexit____doc__,
"__aexit__($self, exc_type=None, exc_value=None, exc_tb=None, /)\n"
"--\n"
"\n"
"Implement async context manager if [Symbol.asyncDispose] is present.");

#define _PYODIDE_CORE_JSPROXY___AEXIT___METHODDEF    \
    {"__aexit__", _PyCFunction_CAST(_pyodide_core_JsProxy___aexit__), METH_FASTCALL, _pyodide_core_JsProxy___aexit____doc__},

static PyObject *
_pyodide_core_JsProxy___aexit___impl(PyObject *self, PyObject *exc_type,
                                     PyObject *exc_value, PyObject *exc_tb);

static PyObject *
_pyodide_core_JsProxy___aexit__(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *exc_type = Py_None;
    PyObject *exc_value = Py_None;
    PyObject *exc_tb = Py_None;

    if (!_PyArg_CheckPositional("__aexit__", nargs, 0, 3)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    exc_type = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    exc_value = args[1];
    if (nargs < 3) {
        goto skip_optional;
    }
    exc_tb = args[2];
skip_optional:
    return_value = _pyodide_core_JsProxy___aexit___impl(self, exc_type, exc_value, exc_tb);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsMap_keys__doc__,
"keys($self, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSMAP_KEYS_METHODDEF    \
    {"keys", (PyCFunction)_pyodide_core_JsMap_keys, METH_NOARGS, _pyodide_core_JsMap_keys__doc__},

static PyObject *
_pyodide_core_JsMap_keys_impl(PyObject *self);

static PyObject *
_pyodide_core_JsMap_keys(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsMap_keys_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsMap_values__doc__,
"values($self, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSMAP_VALUES_METHODDEF    \
    {"values", (PyCFunction)_pyodide_core_JsMap_values, METH_NOARGS, _pyodide_core_JsMap_values__doc__},

static PyObject *
_pyodide_core_JsMap_values_impl(PyObject *self);

static PyObject *
_pyodide_core_JsMap_values(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsMap_values_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsMap_items__doc__,
"items($self, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSMAP_ITEMS_METHODDEF    \
    {"items", (PyCFunction)_pyodide_core_JsMap_items, METH_NOARGS, _pyodide_core_JsMap_items__doc__},

static PyObject *
_pyodide_core_JsMap_items_impl(PyObject *self);

static PyObject *
_pyodide_core_JsMap_items(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsMap_items_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsMap_get__doc__,
"get($self, /, key, default=None)\n"
"--\n"
"\n"
"Return the value for key if key is in the dictionary, else default.");

#define _PYODIDE_CORE_JSMAP_GET_METHODDEF    \
    {"get", _PyCFunction_CAST(_pyodide_core_JsMap_get), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsMap_get__doc__},

static PyObject *
_pyodide_core_JsMap_get_impl(PyObject *self, PyObject *key,
                             PyObject *default_value);

static PyObject *
_pyodide_core_JsMap_get(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 2
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(key), &_Py_ID(default), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"key", "default", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "get",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[2];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *key;
    PyObject *default_value = Py_None;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 2, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    key = args[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    default_value = args[1];
skip_optional_pos:
    return_value = _pyodide_core_JsMap_get_impl(self, key, default_value);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsMap_pop__doc__,
"pop($self, key, default=<unrepresentable>, /)\n"
"--\n"
"\n"
"Return the value for key if key is in the Map, else default.");

#define _PYODIDE_CORE_JSMAP_POP_METHODDEF    \
    {"pop", _PyCFunction_CAST(_pyodide_core_JsMap_pop), METH_FASTCALL, _pyodide_core_JsMap_pop__doc__},

static PyObject *
_pyodide_core_JsMap_pop_impl(PyObject *self, PyObject *key,
                             PyObject *default_value);

static PyObject *
_pyodide_core_JsMap_pop(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *key;
    PyObject *default_value = NULL;

    if (!_PyArg_CheckPositional("pop", nargs, 1, 2)) {
        goto exit;
    }
    key = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    default_value = args[1];
skip_optional:
    return_value = _pyodide_core_JsMap_pop_impl(self, key, default_value);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsMap_popitem__doc__,
"popitem($self, /)\n"
"--\n"
"\n"
"Remove and return a (key, value) pair as a 2-tuple.");

#define _PYODIDE_CORE_JSMAP_POPITEM_METHODDEF    \
    {"popitem", (PyCFunction)_pyodide_core_JsMap_popitem, METH_NOARGS, _pyodide_core_JsMap_popitem__doc__},

static PyObject *
_pyodide_core_JsMap_popitem_impl(PyObject *self);

static PyObject *
_pyodide_core_JsMap_popitem(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsMap_popitem_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsMap_clear__doc__,
"clear($self, /)\n"
"--\n"
"\n"
"Remove all items from the dict.");

#define _PYODIDE_CORE_JSMAP_CLEAR_METHODDEF    \
    {"clear", (PyCFunction)_pyodide_core_JsMap_clear, METH_NOARGS, _pyodide_core_JsMap_clear__doc__},

static PyObject *
_pyodide_core_JsMap_clear_impl(PyObject *self);

static PyObject *
_pyodide_core_JsMap_clear(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsMap_clear_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsMap_setdefault__doc__,
"setdefault($self, key, default=None, /)\n"
"--\n"
"\n"
"Insert key with a value of default if key is not in the Map.\n"
"\n"
"Return the value for key if key is in the Map, else default.");

#define _PYODIDE_CORE_JSMAP_SETDEFAULT_METHODDEF    \
    {"setdefault", _PyCFunction_CAST(_pyodide_core_JsMap_setdefault), METH_FASTCALL, _pyodide_core_JsMap_setdefault__doc__},

static PyObject *
_pyodide_core_JsMap_setdefault_impl(PyObject *self, PyObject *key,
                                    PyObject *default_value);

static PyObject *
_pyodide_core_JsMap_setdefault(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    PyObject *key;
    PyObject *default_value = Py_None;

    if (!_PyArg_CheckPositional("setdefault", nargs, 1, 2)) {
        goto exit;
    }
    key = args[0];
    if (nargs < 2) {
        goto skip_optional;
    }
    default_value = args[1];
skip_optional:
    return_value = _pyodide_core_JsMap_setdefault_impl(self, key, default_value);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray_extend__doc__,
"extend($self, /, iterable)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSARRAY_EXTEND_METHODDEF    \
    {"extend", _PyCFunction_CAST(_pyodide_core_JsArray_extend), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsArray_extend__doc__},

static PyObject *
_pyodide_core_JsArray_extend_impl(PyObject *self, PyObject *iterable);

static PyObject *
_pyodide_core_JsArray_extend(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(iterable), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"iterable", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "extend",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *iterable;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    iterable = args[0];
    return_value = _pyodide_core_JsArray_extend_impl(self, iterable);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray_append__doc__,
"append($self, arg, /)\n"
"--\n"
"\n"
"Append object to the end of the list.");

#define _PYODIDE_CORE_JSARRAY_APPEND_METHODDEF    \
    {"append", (PyCFunction)_pyodide_core_JsArray_append, METH_O, _pyodide_core_JsArray_append__doc__},

PyDoc_STRVAR(_pyodide_core_JsArray_pop__doc__,
"pop($self, index=-1, /)\n"
"--\n"
"\n"
"Remove and return item at index (default last).\n"
"\n"
"Raises IndexError if list is empty or index is out of range");

#define _PYODIDE_CORE_JSARRAY_POP_METHODDEF    \
    {"pop", _PyCFunction_CAST(_pyodide_core_JsArray_pop), METH_FASTCALL, _pyodide_core_JsArray_pop__doc__},

static PyObject *
_pyodide_core_JsArray_pop_impl(PyObject *self, Py_ssize_t index);

static PyObject *
_pyodide_core_JsArray_pop(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t index = -1;

    if (!_PyArg_CheckPositional("pop", nargs, 0, 1)) {
        goto exit;
    }
    if (nargs < 1) {
        goto skip_optional;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        index = ival;
    }
skip_optional:
    return_value = _pyodide_core_JsArray_pop_impl(self, index);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray___reversed____doc__,
"__reversed__($self, /)\n"
"--\n"
"\n"
"Return a reverse iterator over the array.");

#define _PYODIDE_CORE_JSARRAY___REVERSED___METHODDEF    \
    {"__reversed__", (PyCFunction)_pyodide_core_JsArray___reversed__, METH_NOARGS, _pyodide_core_JsArray___reversed____doc__},

static PyObject *
_pyodide_core_JsArray___reversed___impl(PyObject *self);

static PyObject *
_pyodide_core_JsArray___reversed__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsArray___reversed___impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsArray_index__doc__,
"index($self, /, value, start=0, stop=sys.maxsize)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSARRAY_INDEX_METHODDEF    \
    {"index", _PyCFunction_CAST(_pyodide_core_JsArray_index), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsArray_index__doc__},

static PyObject *
_pyodide_core_JsArray_index_impl(PyObject *self, PyObject *value,
                                 Py_ssize_t start, Py_ssize_t stop);

static PyObject *
_pyodide_core_JsArray_index(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 3
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(value), &_Py_ID(start), &_Py_ID(stop), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"value", "start", "stop", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "index",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *value;
    Py_ssize_t start = 0;
    Py_ssize_t stop = PY_SSIZE_T_MAX;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 3, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    value = args[0];
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[1]) {
        if (!_PyEval_SliceIndexNotNone(args[1], &start)) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_pos;
        }
    }
    if (!_PyEval_SliceIndexNotNone(args[2], &stop)) {
        goto exit;
    }
skip_optional_pos:
    return_value = _pyodide_core_JsArray_index_impl(self, value, start, stop);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray_count__doc__,
"count($self, /, value)\n"
"--\n"
"\n"
"Return number of occurrences of value.");

#define _PYODIDE_CORE_JSARRAY_COUNT_METHODDEF    \
    {"count", _PyCFunction_CAST(_pyodide_core_JsArray_count), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsArray_count__doc__},

static PyObject *
_pyodide_core_JsArray_count_impl(PyObject *self, PyObject *value);

static PyObject *
_pyodide_core_JsArray_count(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(value), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"value", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "count",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    PyObject *value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    value = args[0];
    return_value = _pyodide_core_JsArray_count_impl(self, value);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray_reverse__doc__,
"reverse($self, /)\n"
"--\n"
"\n"
"Return number of occurrences of value.");

#define _PYODIDE_CORE_JSARRAY_REVERSE_METHODDEF    \
    {"reverse", (PyCFunction)_pyodide_core_JsArray_reverse, METH_NOARGS, _pyodide_core_JsArray_reverse__doc__},

static PyObject *
_pyodide_core_JsArray_reverse_impl(PyObject *self);

static PyObject *
_pyodide_core_JsArray_reverse(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsArray_reverse_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsArray_insert__doc__,
"insert($self, index, object, /)\n"
"--\n"
"\n"
"Insert object before index.");

#define _PYODIDE_CORE_JSARRAY_INSERT_METHODDEF    \
    {"insert", _PyCFunction_CAST(_pyodide_core_JsArray_insert), METH_FASTCALL, _pyodide_core_JsArray_insert__doc__},

static PyObject *
_pyodide_core_JsArray_insert_impl(PyObject *self, Py_ssize_t index,
                                  PyObject *object);

static PyObject *
_pyodide_core_JsArray_insert(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    PyObject *return_value = NULL;
    Py_ssize_t index;
    PyObject *object;

    if (!_PyArg_CheckPositional("insert", nargs, 2, 2)) {
        goto exit;
    }
    {
        Py_ssize_t ival = -1;
        PyObject *iobj = _PyNumber_Index(args[0]);
        if (iobj != NULL) {
            ival = PyLong_AsSsize_t(iobj);
            Py_DECREF(iobj);
        }
        if (ival == -1 && PyErr_Occurred()) {
            goto exit;
        }
        index = ival;
    }
    object = args[1];
    return_value = _pyodide_core_JsArray_insert_impl(self, index, object);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsArray_remove__doc__,
"remove($self, value, /)\n"
"--\n"
"\n"
"Remove first occurrence of value.\n"
"\n"
"Raises ValueError if the value is not present.");

#define _PYODIDE_CORE_JSARRAY_REMOVE_METHODDEF    \
    {"remove", (PyCFunction)_pyodide_core_JsArray_remove, METH_O, _pyodide_core_JsArray_remove__doc__},

PyDoc_STRVAR(_pyodide_core_JsBuffer_assign_to__doc__,
"assign_to($self, target, /)\n"
"--\n"
"\n"
"Copy the data from the TypedArray into the Python buffer target.\n"
"\n"
"target must support the buffer protocol, be writable, and have the same\n"
"length as the TypedArray. Does not change the length of the buffer.");

#define _PYODIDE_CORE_JSBUFFER_ASSIGN_TO_METHODDEF    \
    {"assign_to", (PyCFunction)_pyodide_core_JsBuffer_assign_to, METH_O, _pyodide_core_JsBuffer_assign_to__doc__},

PyDoc_STRVAR(_pyodide_core_JsBuffer_assign__doc__,
"assign($self, source, /)\n"
"--\n"
"\n"
"Copy the data from the Python buffer source into the TypedArray.\n"
"\n"
"source must support the buffer protocol and have the same length as the\n"
"TypedArray. Does not change the length of the TypedArray.");

#define _PYODIDE_CORE_JSBUFFER_ASSIGN_METHODDEF    \
    {"assign", (PyCFunction)_pyodide_core_JsBuffer_assign, METH_O, _pyodide_core_JsBuffer_assign__doc__},

PyDoc_STRVAR(_pyodide_core_JsBuffer_to_memoryview__doc__,
"to_memoryview($self, /)\n"
"--\n"
"\n"
"Convert a buffer to a memoryview.\n"
"\n"
"Copies the data once. This currently has the same effect as to_py.");

#define _PYODIDE_CORE_JSBUFFER_TO_MEMORYVIEW_METHODDEF    \
    {"to_memoryview", (PyCFunction)_pyodide_core_JsBuffer_to_memoryview, METH_NOARGS, _pyodide_core_JsBuffer_to_memoryview__doc__},

static PyObject *
_pyodide_core_JsBuffer_to_memoryview_impl(PyObject *self);

static PyObject *
_pyodide_core_JsBuffer_to_memoryview(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsBuffer_to_memoryview_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsBuffer_to_bytes__doc__,
"to_bytes($self, /)\n"
"--\n"
"\n"
"Convert a buffer to a bytes object.\n"
"\n"
"Copies the data once.");

#define _PYODIDE_CORE_JSBUFFER_TO_BYTES_METHODDEF    \
    {"to_bytes", (PyCFunction)_pyodide_core_JsBuffer_to_bytes, METH_NOARGS, _pyodide_core_JsBuffer_to_bytes__doc__},

static PyObject *
_pyodide_core_JsBuffer_to_bytes_impl(PyObject *self);

static PyObject *
_pyodide_core_JsBuffer_to_bytes(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsBuffer_to_bytes_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsBuffer_to_string__doc__,
"to_string($self, /, encoding=None)\n"
"--\n"
"\n"
"Convert a buffer to a string object.\n"
"\n"
"  encoding\n"
"    The encoding to use. Default is utf8. The encoding is passed to the\n"
"    JavaScript TextDecoder so any encoding supported by TextDecoder may be\n"
"    used.\n"
"\n"
"Copies the data twice.");

#define _PYODIDE_CORE_JSBUFFER_TO_STRING_METHODDEF    \
    {"to_string", _PyCFunction_CAST(_pyodide_core_JsBuffer_to_string), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_JsBuffer_to_string__doc__},

static PyObject *
_pyodide_core_JsBuffer_to_string_impl(PyObject *self, const char *encoding);

static PyObject *
_pyodide_core_JsBuffer_to_string(PyObject *self, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 1
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(encoding), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"encoding", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "to_string",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    const char *encoding = NULL;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 0, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (args[0] == Py_None) {
        encoding = NULL;
    }
    else if (PyUnicode_Check(args[0])) {
        encoding = _PyUnicode_AsUTF8NoNUL(args[0]);
        if (encoding == NULL) {
            goto exit;
        }
    }
    else {
        _PyArg_BadArgument("to_string", "argument 'encoding'", "str or None", args[0]);
        goto exit;
    }
skip_optional_pos:
    return_value = _pyodide_core_JsBuffer_to_string_impl(self, encoding);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_JsBuffer_to_file__doc__,
"to_file($self, file, /)\n"
"--\n"
"\n"
"Writes a buffer to a file.\n"
"\n"
"Will write the entire contents of the buffer to the current position of\n"
"the file. file must be a file object with a fileno() method.");

#define _PYODIDE_CORE_JSBUFFER_TO_FILE_METHODDEF    \
    {"to_file", (PyCFunction)_pyodide_core_JsBuffer_to_file, METH_O, _pyodide_core_JsBuffer_to_file__doc__},

PyDoc_STRVAR(_pyodide_core_JsBuffer_from_file__doc__,
"from_file($self, file, /)\n"
"--\n"
"\n"
"Reads from a file into a buffer.\n"
"\n"
"Will try to read a chunk of data the same size as the buffer from the\n"
"current position of the file. file must be a file object with a fileno()\n"
"method.");

#define _PYODIDE_CORE_JSBUFFER_FROM_FILE_METHODDEF    \
    {"from_file", (PyCFunction)_pyodide_core_JsBuffer_from_file, METH_O, _pyodide_core_JsBuffer_from_file__doc__},

PyDoc_STRVAR(_pyodide_core_JsBuffer__into_file__doc__,
"_into_file($self, file, /)\n"
"--\n"
"\n"
"Will write the entire contents of a buffer into a file.\n"
"\n"
"The buffer must be empty afterwards, i.e., the file must be opened as a\n"
"fresh empty file in the Emscripten MEMFS file system. The purpose is to\n"
"avoid a copy of the data: the backing ArrayBuffer is handed over to the\n"
"file system.");

#define _PYODIDE_CORE_JSBUFFER__INTO_FILE_METHODDEF    \
    {"_into_file", (PyCFunction)_pyodide_core_JsBuffer__into_file, METH_O, _pyodide_core_JsBuffer__into_file__doc__},

PyDoc_STRVAR(_pyodide_core_JsException___reduce____doc__,
"__reduce__($self, /)\n"
"--\n"
"\n");

#define _PYODIDE_CORE_JSEXCEPTION___REDUCE___METHODDEF    \
    {"__reduce__", (PyCFunction)_pyodide_core_JsException___reduce__, METH_NOARGS, _pyodide_core_JsException___reduce____doc__},

static PyObject *
_pyodide_core_JsException___reduce___impl(PyObject *self);

static PyObject *
_pyodide_core_JsException___reduce__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsException___reduce___impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsDoubleProxy_unwrap__doc__,
"unwrap($self, /)\n"
"--\n"
"\n"
"Unwrap a double proxy created with create_proxy.");

#define _PYODIDE_CORE_JSDOUBLEPROXY_UNWRAP_METHODDEF    \
    {"unwrap", (PyCFunction)_pyodide_core_JsDoubleProxy_unwrap, METH_NOARGS, _pyodide_core_JsDoubleProxy_unwrap__doc__},

static PyObject *
_pyodide_core_JsDoubleProxy_unwrap_impl(PyObject *self);

static PyObject *
_pyodide_core_JsDoubleProxy_unwrap(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsDoubleProxy_unwrap_impl(self);
}

PyDoc_STRVAR(_pyodide_core_JsProxy_as_py_json__doc__,
"as_py_json($self, /)\n"
"--\n"
"\n"
"Return a new JsProxy that treats a JavaScript object as Python json.\n"
"\n"
"It allows one to treat a JavaScript object that is a mixture of\n"
"JavaScript arrays and objects as a mixture of Python lists and dicts.");

#define _PYODIDE_CORE_JSPROXY_AS_PY_JSON_METHODDEF    \
    {"as_py_json", (PyCFunction)_pyodide_core_JsProxy_as_py_json, METH_NOARGS, _pyodide_core_JsProxy_as_py_json__doc__},

static PyObject *
_pyodide_core_JsProxy_as_py_json_impl(PyObject *self);

static PyObject *
_pyodide_core_JsProxy_as_py_json(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsProxy_as_py_json_impl(self);
}
/*[clinic end generated code: output=37612e02633c10c7 input=a9049054013a1b77]*/
