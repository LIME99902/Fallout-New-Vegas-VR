#!/bin/sh
# Builds FNVR.dll (32-bit Windows DLL) with an llvm-mingw toolchain.
# Get one from https://github.com/mstorsjo/llvm-mingw/releases and point TC at its bin folder.
set -e
TC=${TC:-/opt/llvm-mingw/bin}
mkdir -p out
$TC/i686-w64-mingw32-gcc -O2 -Wall -Wextra -shared -static -s \
  -Wl,--kill-at -o out/FNVR.dll src/fnvr.c -ld3d11 -ldxgi -ldxguid -luuid -lshell32 -lgdi32
