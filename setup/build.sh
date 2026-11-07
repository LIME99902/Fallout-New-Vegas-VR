#!/bin/sh
# builds FNVVRMC-Setup.exe (64-bit Windows console program) with llvm-mingw
TC=${TC:-/home/claude/tools/llvm-mingw-20250528-ucrt-ubuntu-22.04-x86_64/bin}
$TC/x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -municode -static -s -o FNVVRMC-Setup.exe setup.c -lwinhttp -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32
