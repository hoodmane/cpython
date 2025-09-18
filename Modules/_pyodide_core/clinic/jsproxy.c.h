/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

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
_pyodide_core_JsProxy___dir___impl(JsProxy *self);

static PyObject *
_pyodide_core_JsProxy___dir__(PyObject *self, PyObject *Py_UNUSED(ignored))
{
    return _pyodide_core_JsProxy___dir___impl((JsProxy *)self);
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
_pyodide_core_JsProxy_to_py_impl(JsProxy *self, int depth,
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
    return_value = _pyodide_core_JsProxy_to_py_impl((JsProxy *)self, depth, default_converter);

exit:
    return return_value;
}
/*[clinic end generated code: output=426a5c99bb7c1a01 input=a9049054013a1b77]*/
