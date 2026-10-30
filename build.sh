#!/bin/sh
# Builds FNVR.dll (32-bit xNVSE plugin) and FNVR_Voice.exe (64-bit offline voice helper).
# Needs llvm-mingw (https://github.com/mstorsjo/llvm-mingw). For the voice helper also set
# VOSK to the unzipped vosk-win64-0.3.45 folder (https://github.com/alphacep/vosk-api/releases/tag/v0.3.45).
set -e
TC=${TC:-/home/claude/tools/llvm-mingw-20250528-ucrt-ubuntu-22.04-x86_64/bin}
mkdir -p out
$TC/i686-w64-mingw32-gcc -O2 -Wall -Wextra -shared -static -s \
  -Wl,--kill-at -o out/FNVR.dll src/fnvr.c -ld3d11 -ldxgi -ldxguid -luuid -lshell32 -lgdi32 -lole32 -lsapi
if [ -n "$VOSK" ]; then
  (cd src && $TC/x86_64-w64-mingw32-windres fnvr_voice.rc -O coff -o ../out/fnvr_voice.res.o)
  $TC/x86_64-w64-mingw32-gcc -O2 -Wall -o out/FNVR_Voice.exe src/fnvr_voice.c out/fnvr_voice.res.o \
    -L"$VOSK" -l:libvosk.dll -lole32 -luuid
  rm -f out/fnvr_voice.res.o
fi
