#include "emscripten.h"
#include "jslib.h"

EM_JS_DEPS(_PyJsvError_Create, "_Py_getJsErrorModule");
EM_JS(JsVal, _PyJsvError_Create, (void), {
    return errorMarker;
}
let errorInstance;
(function () {
  const isIOS =
    globalThis.navigator &&
    (/iPad|iPhone|iPod/.test(navigator.userAgent) ||
      // Starting with iPadOS 13, iPads might send a platform string that looks like a desktop Mac.
      // To differentiate, we check if the platform is 'MacIntel' (common for Macs and newer iPads)
      // AND if the device has multi-touch capabilities (navigator.maxTouchPoints > 1)
      (navigator.platform === "MacIntel" &&
        typeof navigator.maxTouchPoints !== "undefined" &&
        navigator.maxTouchPoints > 1));
  if (isIOS) {
    return;
  }
  try {
    const errorModule = _Py_getJsErrorModule();
    errorInstance = new WebAssembly.Instance(errorModule);
    _PyJsvError_Create = errorInstance.exports.JsvError_Create;
  } catch (e) {}
})();
let errorMarker;
if (!errorInstance) {
  errorMarker = Symbol("errorMarker");
}
);

EM_JS_DEPS(_PyJsvError_Check, "_PyJsvError_Create");
EM_JS(int, _PyJsvError_Check, (JsVal x), {
    return x === errorMarker;
}
if (errorInstance) {
    _PyJsvError_Check = errorInstance.exports.JsvError_Check;
}
);