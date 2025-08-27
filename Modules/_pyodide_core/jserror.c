#include "emscripten.h"
#include "jslib.h"

EM_JS_DEPS(JsvError_Create, "getJsErrorModule");
EM_JS(JsVal, JsvError_Create, (void), {
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
    const errorModule = getJsErrorModule();
    errorInstance = new WebAssembly.Instance(errorModule);
    JsvError_Create = errorInstance.exports.JsvError_Create;
  } catch (e) {}
})();
let errorMarker;
if (!errorInstance) {
  errorMarker = Symbol("errorMarker");
}
);

EM_JS_DEPS(JsvError_Check, "JsvError_Create");
EM_JS(int, JsvError_Check, (JsVal x), {
    return x === errorMarker;
}
if (errorInstance) {
    JsvError_Check = errorInstance.exports.JsvError_Check;
}
);