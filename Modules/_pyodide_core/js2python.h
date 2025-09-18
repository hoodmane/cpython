#include "jslib.h"

/**
 * Convert a JavaScript object to a Python object.
 *  \param x The JavaScript object.
 *  \return The Python object resulting from the conversion. Returns NULL and
 *    sets the Python error indicator if a conversion error occurs.
 */
PyObject*
_Py_js2python(JsVal x);

/**
 * Do a deep conversion
 */
PyObject*
_Py_js2python_convert(JsVal x, int depth, JsVal defaultConverter);
