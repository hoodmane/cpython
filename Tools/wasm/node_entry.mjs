import EmscriptenModule from "./python.mjs";
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
    
const settings = {
  preRun(Module) {
    const __dirname = dirname(fileURLToPath(import.meta.url));
    Module.FS.mkdirTree("/lib/");
    Module.FS.mount(Module.FS.filesystems.NODEFS, { root: __dirname + "/lib/" }, "/lib/");
  }
};

await EmscriptenModule(settings);
