#!/bin/bash
set +e

export CFLAGS="-O2 -fPIC"

# Build paths
export CPATH="$PREFIX/include"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

CC=emcc EMSCRIPTEN_DEDUPLICATE=1 EXTERN_FAIL=1 make

cp dist/lib/* $PREFIX/lib/
cp dist/include/* $PREFIX/include/

echo prefix=$PREFIX > $PREFIX/lib/pkgconfig/libhiwire.pc
cat $EMSCRIPTEN_DIR/libhiwire.pc >> $PREFIX/lib/pkgconfig/libhiwire.pc

