import { build } from "esbuild";

try {
  await build({
    entryPoints: ["pyproxy.gen.ts"],
    outfile: "pyproxy.gen.js",
    format: "iife",
    keepNames: true,
    sourcemap: true,
    bundle: true,
  });
} catch ({ message }) {
  console.error(message);
  process.exit(1);
}
