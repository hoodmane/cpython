/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(_pyodide_core_bad_hiwire_get__doc__,
"bad_hiwire_get($module, select, /)\n"
"--\n"
"\n"
"Run hiwire_get on bad imputs to test the errors.\n"
"\n"
"Currently this fatally crashes the runtime so it can\'t be used in a test.");

#define _PYODIDE_CORE_BAD_HIWIRE_GET_METHODDEF    \
    {"bad_hiwire_get", (PyCFunction)_pyodide_core_bad_hiwire_get, METH_O, _pyodide_core_bad_hiwire_get__doc__},

static PyObject *
_pyodide_core_bad_hiwire_get_impl(PyObject *module, int select);

static PyObject *
_pyodide_core_bad_hiwire_get(PyObject *module, PyObject *arg)
{
    PyObject *return_value = NULL;
    int select;

    select = PyLong_AsInt(arg);
    if (select == -1 && PyErr_Occurred()) {
        goto exit;
    }
    return_value = _pyodide_core_bad_hiwire_get_impl(module, select);

exit:
    return return_value;
}
/*[clinic end generated code: output=83f3425097ed982f input=a9049054013a1b77]*/
