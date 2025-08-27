#!/usr/bin/env python3

import argparse
import sys
import tempfile
from shutil import which
from pathlib import Path
import subprocess
from contextlib import contextmanager

JS_TEMPLATE = """
#include "emscripten.h"

EM_JS(void, {function_name}, (void), {{
    return new WebAssembly.Module(hexStringToUTF8Array("{hex_string}"));
}})
"""

def find_wasm_as():
    emcc_path = which("emcc")
    if not emcc_path:
        print("Error: emcc not found in PATH", file=sys.stderr)
        return None

    wasm_as_path = Path(emcc_path).parents[1] / "bin/wasm-as"

    if not wasm_as_path.exists():
        print(f"Error: wasm-as not found at {wasm_as_path}", file=sys.stderr)
        return None
    return wasm_as_path


def encode_js_file(input_file, output_file, function_name):
    # Read the compiled WASM as binary and convert to hex
    wasm_bytes = input_file.read_bytes()

    hex_string = "".join(f"{byte:02x}" for byte in wasm_bytes)

    # Generate JavaScript module
    js_content = JS_TEMPLATE.format(
        function_name=function_name, hex_string=hex_string
    )
    output_file.write_text(js_content)

    print(f"Successfully compiled {input_file} and generated {output_file}")
    return 0

@contextmanager
def assemble_file(input_file):
    wasm_as_path = find_wasm_as()
    if wasm_as_path is None:
        return 1

    with tempfile.NamedTemporaryFile(
        suffix=".wasm", delete=False
    ) as temp_wasm:
        temp_wasm_path = Path(temp_wasm.name)

    try:
        subprocess.run(
            [wasm_as_path, input_file, "-o", temp_wasm_path, "-all"],
            check=True,
        )

        yield temp_wasm_path
    except subprocess.CalledProcessError as e:
        print(f"Error compiling {input_file}: {e}", file=sys.stderr)
        return 1
    finally:
        # Clean up temporary file
        temp_wasm_path.unlink(missing_ok=True)


def prepare_wasm(input_file, output_file, function_name):
    if input_file.suffix == ".wasm":
        return encode_js_file(input_file, output_file, function_name)
    if input_file.suffix == ".wat":
        with assemble_file(input_file) as temp:
            return encode_js_file(temp, output_file, function_name)
    raise ValueError("Unexpected file extension")


def main():
    parser = argparse.ArgumentParser(
        description="Compile WebAssembly text files using wasm-as"
    )
    parser.add_argument("input_file", help="Input .wat file to compile")
    parser.add_argument("output_file", help="Output file name")
    parser.add_argument("function_name", help="Name of the export function")

    args = parser.parse_args()

    return prepare_wasm(Path(args.input_file), Path(args.output_file), args.function_name)


if __name__ == "__main__":
    sys.exit(main())
