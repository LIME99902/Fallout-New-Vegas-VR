#!/bin/sh
set -e
TC=/home/claude/tools/llvm-mingw-20250528-ucrt-ubuntu-22.04-x86_64/bin
mkdir -p out
$TC/i686-w64-mingw32-gcc -O2 -Wall -Wextra -shared -static -s \
  -Wl,--kill-at -o out/FNVR.dll src/fnvr.c -ld3d11 -ldxgi -ldxguid -luuid -lshell32 -lgdi32 -lole32 -lsapi
