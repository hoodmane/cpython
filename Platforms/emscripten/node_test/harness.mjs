// Test harness for driving the Emscripten Python build from JavaScript with
// node's builtin test runner. This lets us test the parts of the JavaScript
// FFI that need the JavaScript event loop to turn (promises, async iteration,
// the WebLoop), which isn't possible from a synchronous Python test.
//
// Run with: node --test Platforms/emscripten/node_test/
//
// The build is located through the PYTHON_MJS environment variable, falling
// back to the default cross-build location.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "../../..",
);
const pythonMjs =
  process.env.PYTHON_MJS ??
  path.join(repoRoot, "cross-build/wasm32-emscripten/build/python/python.mjs");

const { default: createEmscriptenModule } = await import(pythonMjs);

export const Module = await createEmscriptenModule({
  noInitialRun: true,
  preRun(Module) {
    // Mount the host file system like node_entry.mjs does.
    for (const dir of fs
      .readdirSync("/")
      .filter((d) => !["dev", "lib", "proc"].includes(d))) {
      Module.FS.mkdirTree("/" + dir);
      Module.FS.mount(Module.FS.filesystems.NODEFS, { root: "/" + dir }, "/" + dir);
    }
    Module.FS.chdir(process.cwd());
    // Make a fake prefix so getpath finds the standard library in the source
    // tree without being given sys.executable.
    const versionInt = Module.HEAPU32[Module._Py_Version >>> 2];
    const major = (versionInt >>> 24) & 0xff;
    const minor = (versionInt >>> 16) & 0xff;
    const stdlib = `/py/lib/python${major}.${minor}`;
    Module.FS.mkdirTree(stdlib);
    Module.FS.mount(
      Module.FS.filesystems.NODEFS,
      { root: path.join(repoRoot, "Lib") },
      stdlib,
    );
    Module.FS.mkdirTree(`${stdlib}/lib-dynload`);
    Module.ENV.PYTHONHOME = "/py";
  },
});

Module._Py_InitializeEx(0);

/**
 * Run Python code with PyRun_SimpleString. Throws if the code raises (the
 * traceback is printed to stderr).
 */
export function runPython(code) {
  const ptr = Module.stringToNewUTF8(code);
  try {
    if (Module._PyRun_SimpleString(ptr) !== 0) {
      throw new Error(`Python raised an exception while running:\n${code}`);
    }
  } finally {
    Module._free(ptr);
  }
}

let counter = 0;

/**
 * Evaluate a Python expression and return its conversion to JavaScript.
 * Python objects with no implicit conversion come back as PyProxies.
 */
export function pyEval(expr) {
  const name = `__pyeval_result_${counter++}`;
  runPython(`import js\njs.globalThis.${name} = (${expr})`);
  const result = globalThis[name];
  delete globalThis[name];
  return result;
}

/**
 * Run Python code in a copy of the __main__ namespace (which has asyncio, js,
 * and pyodide.ffi helpers imported) and return an object mapping the given
 * names to the JavaScript conversions of the corresponding variables.
 */
export function pyRun(code, ...names) {
  const ns = `__pyrun_ns_${counter++}`;
  runPython(`${ns} = dict(globals())\nexec(${JSON.stringify(code)}, ${ns})`);
  const result = {};
  for (const name of names) {
    result[name] = pyEval(`${ns}[${JSON.stringify(name)}]`);
  }
  runPython(`del ${ns}`);
  return result;
}

export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

runPython(`
import asyncio
import js
from pyodide.ffi import create_proxy, create_once_callable, to_js, JsException
from pyodide.webloop import _initialize_event_loop, WebLoop, PyodideFuture, PyodideTask

loop = _initialize_event_loop()
`);
