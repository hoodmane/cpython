/*[clinic input]
preserve
[clinic start generated code]*/

#if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)
#  include "pycore_gc.h"          // PyGC_Head
#  include "pycore_runtime.h"     // _Py_ID()
#endif
#include "pycore_modsupport.h"    // _PyArg_UnpackKeywords()

PyDoc_STRVAR(_pyodide_core_to_js__doc__,
"to_js($module, obj, /, *, depth=-1, pyproxies=None,\n"
"      create_pyproxies=True, dict_converter=None,\n"
"      default_converter=None, eager_converter=None)\n"
"--\n"
"\n"
"Convert the object to JavaScript.\n"
"\n"
"This is similar to pyodide.ffi.PyProxy.toJs, but for use from Python. If\n"
"the object can be implicitly translated to JavaScript, it will be returned\n"
"unchanged. If the object cannot be converted into JavaScript, this method\n"
"will return a JsProxy of a pyodide.ffi.PyProxy, as if you had used\n"
"pyodide.ffi.create_proxy.\n"
"\n"
"See :ref:`type-translations-pyproxy-to-js` for more information.");

#define _PYODIDE_CORE_TO_JS_METHODDEF    \
    {"to_js", _PyCFunction_CAST(_pyodide_core_to_js), METH_FASTCALL|METH_KEYWORDS, _pyodide_core_to_js__doc__},

static PyObject *
_pyodide_core_to_js_impl(PyObject *module, PyObject *obj, int depth,
                         PyObject *pyproxies, int create_pyproxies,
                         PyObject *dict_converter,
                         PyObject *default_converter,
                         PyObject *eager_converter);

static PyObject *
_pyodide_core_to_js(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    #if defined(Py_BUILD_CORE) && !defined(Py_BUILD_CORE_MODULE)

    #define NUM_KEYWORDS 6
    static struct {
        PyGC_Head _this_is_not_used;
        PyObject_VAR_HEAD
        Py_hash_t ob_hash;
        PyObject *ob_item[NUM_KEYWORDS];
    } _kwtuple = {
        .ob_base = PyVarObject_HEAD_INIT(&PyTuple_Type, NUM_KEYWORDS)
        .ob_hash = -1,
        .ob_item = { &_Py_ID(depth), &_Py_ID(pyproxies), &_Py_ID(create_pyproxies), &_Py_ID(dict_converter), &_Py_ID(default_converter), &_Py_ID(eager_converter), },
    };
    #undef NUM_KEYWORDS
    #define KWTUPLE (&_kwtuple.ob_base.ob_base)

    #else  // !Py_BUILD_CORE
    #  define KWTUPLE NULL
    #endif  // !Py_BUILD_CORE

    static const char * const _keywords[] = {"", "depth", "pyproxies", "create_pyproxies", "dict_converter", "default_converter", "eager_converter", NULL};
    static _PyArg_Parser _parser = {
        .keywords = _keywords,
        .fname = "to_js",
        .kwtuple = KWTUPLE,
    };
    #undef KWTUPLE
    PyObject *argsbuf[7];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 1;
    PyObject *obj;
    int depth = -1;
    PyObject *pyproxies = Py_None;
    int create_pyproxies = 1;
    PyObject *dict_converter = Py_None;
    PyObject *default_converter = Py_None;
    PyObject *eager_converter = Py_None;

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
        depth = PyLong_AsInt(args[1]);
        if (depth == -1 && PyErr_Occurred()) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[2]) {
        pyproxies = args[2];
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[3]) {
        create_pyproxies = PyObject_IsTrue(args[3]);
        if (create_pyproxies < 0) {
            goto exit;
        }
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[4]) {
        dict_converter = args[4];
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    if (args[5]) {
        default_converter = args[5];
        if (!--noptargs) {
            goto skip_optional_kwonly;
        }
    }
    eager_converter = args[6];
skip_optional_kwonly:
    return_value = _pyodide_core_to_js_impl(module, obj, depth, pyproxies, create_pyproxies, dict_converter, default_converter, eager_converter);

exit:
    return return_value;
}

PyDoc_STRVAR(_pyodide_core_destroy_proxies__doc__,
"destroy_proxies($module, arg, /)\n"
"--\n"
"\n"
"Destroy all PyProxies in a JavaScript array.\n"
"\n"
"pyproxies must be a JavaScript Array of PyProxies. Intended for use\n"
"with the arrays created from the \"pyproxies\" argument of PyProxy.toJs\n"
"and to_js. This method is necessary because indexing the Array from\n"
"Python automatically unwraps the PyProxy into the wrapped Python object.");

#define _PYODIDE_CORE_DESTROY_PROXIES_METHODDEF    \
    {"destroy_proxies", (PyCFunction)_pyodide_core_destroy_proxies, METH_O, _pyodide_core_destroy_proxies__doc__},
/*[clinic end generated code: output=62e575063a38b06f input=a9049054013a1b77]*/
