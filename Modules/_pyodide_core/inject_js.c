#include <emscripten.h>

__attribute__((used))
__attribute__((section("em_js"),
               aligned(1))) char __em_js__pyodide_js_init[] = {
        // "()=>{}" in ascii.
    40, 41, 60, 58, 58, 62, 123, 125,
    #embed "jslib.js"
    ,
    #embed "pyproxy.gen.js"
    // Null byte to end string
    , 0
};

void
pyodide_js_init(void) EM_IMPORT(pyodide_js_init);

EMSCRIPTEN_KEEPALIVE void
pyodide_export(void)
{
  pyodide_js_init();
}
