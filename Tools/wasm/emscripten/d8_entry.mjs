import EmscriptenModule from "./python.mjs";

const argsv = arguments;

function mountStdlib(Module) {
  Module.FS.mkdirTree(`/lib/python3.14/lib-dynload/`);
  // Copy standard library from native fs into memfs
  const buf = readbuffer("python3.14.zip");
  Module.FS.writeFile(`/lib/python314.zip`, new Uint8Array(buf), {
    canOwn: true,
  });
  for (const arg of argsv) {
    // If any argument is a file, copy it into the memfs.
    // This makes python with a script argument work.
    try {
      const buf = readbuffer(arg);
      Module.FS.writeFile(arg, new Uint8Array(buf), { canOwn: true });
    } catch (e) {}
  }
}

const settings = {
  preRun(Module) {
    // Globally expose API object so we can access it if we raise on startup.
    globalThis.Module = Module;
    mountStdlib(Module);
  },
  arguments,
};

try {
  await EmscriptenModule(settings);
} catch (e) {
  // Show JavaScript exception and traceback
  console.warn(e);
  // Show Python exception and traceback
  Module.__Py_DumpTraceback(2, Module._PyGILState_GetThisThreadState());
  throw e;
}
