import EmscriptenModule from "./python.mjs";
import { dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { readdirSync } from "node:fs";

function rootDirsToMount() {
  const skipDirs = ["dev", "lib", "proc", "tmp"];
  return readdirSync("/")
    .filter((dir) => !skipDirs.includes(dir))
    .map((dir) => "/" + dir);
}

function dirsToMount() {
  const extra_mounts = process.env["_EMSCRIPTEN_EXTRA_MOUNTS"] || "";
  return rootDirsToMount().concat(extra_mounts.split(":").filter((s) => s));
}

const settings = {
  preRun(Module) {
    const __dirname = dirname(fileURLToPath(import.meta.url));
    Object.assign(Module.ENV, process.env);
    Module.FS.mkdirTree("/lib/");
    Module.FS.mount(
      Module.FS.filesystems.NODEFS,
      { root: __dirname + "/lib/" },
      "/lib/",
    );
    for (const mount of dirsToMount()) {
      Module.FS.mkdirTree(mount);
      Module.FS.mount(Module.FS.filesystems.NODEFS, { root: mount }, mount);
    }
    Module.FS.chdir(process.cwd());
  },
};

await EmscriptenModule(settings);
