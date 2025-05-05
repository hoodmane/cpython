#include "Python.h"
#include "error_handling.h"
#include <emscripten.h>

/**
 * Set Python error indicator from JavaScript.
 */
EMSCRIPTEN_KEEPALIVE void
set_error(char* err)
{
  PyErr_SetString(PyExc_RuntimeError, err);
}

EM_JS(void, error_handling_init_js, (void), {
  Module.handle_js_error = function (e) {
    const estr = e.toString();
    withStackSave(() => {
        _set_error(stringToUTF8OnStack(estr));
    });
  };
}
const nullToUndefined = (x) => (x === null ? undefined : x);
);

__attribute__((constructor)) void error_handling_init(void) {
  error_handling_init_js();
}

#ifdef DEBUG_F
EM_JS(void, console_error, (char* msg), {
  let jsmsg = UTF8ToString(msg);
  console.error(jsmsg);
});
#endif

