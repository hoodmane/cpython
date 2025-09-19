/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(_pyodide_core_create_proxy__doc__,
"create_proxy($module, obj, /, *, capture_this=False, roundtrip=True)\n"
"--\n"
"\n"
"Create a JsProxy of a pyodide.ffi.PyProxy.\n"
"\n"
"  obj\n"
"    The object to wrap.\n"
"  capture_this\n"
"    If the object is callable, should ``this`` be passed as the first\n"
"    argument when calling it from JavaScript.\n"
"  roundtrip\n"
"    When the proxy is converted back from JavaScript to Python, if this is\n"
"    True it is converted into a double proxy. If False, it is\n"
"    unwrapped into a Python object. In the case that roundtrip is\n"
"    True it is possible to unwrap a double proxy with the\n"
"    JsDoubleProxy.unwrap method. This is useful to allow easier\n"
"    control of lifetimes from Python:\n"
"\n"
"This allows explicit control over the lifetime of the PyProxy from Python.\n"
"Call JsDoubleProxy.destroy API when done.");

#define _PYODIDE_CORE_CREATE_PROXY_METHODDEF    \
    {"create_proxy", _PyCFunction_CAST(_pyodide_core_create_proxy), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_create_proxy__doc__},

static PyObject *
_pyodide_core_create_proxy_impl(PyObject *module, PyObject *obj,
                                int capture_this, int roundtrip);

static PyObject *
_pyodide_core_create_proxy(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
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
        .ob_item = { &_Py_ID(capture_this), &_Py_ID(roundtrip), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"", "capture_this", "roundtrip", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "create_proxy",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[3];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *obj;
    int capture_this = 0;
    int roundtrip = 1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser,
            /*minpos*/ 1, /*maxpos*/ 1, /*minkw*/ 0, /*varpos*/ 0, argsbuf);
    if (!args) {
        goto exit;
    }
    obj = args[0];
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    if (args[1]) {
        capture_this = PyObject_IsTrue(args[1]);
        if (capture_this < 0) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    roundtrip = PyObject_IsTrue(args[2]);
    if (roundtrip < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = _pyodide_core_create_proxy_impl(module, obj, capture_this, roundtrip);

exit:
    return return_value;
}
/*[clinic end generated code: output=5b0b8fe3f9ea1d34 input=a9049054013a1b77]*/
