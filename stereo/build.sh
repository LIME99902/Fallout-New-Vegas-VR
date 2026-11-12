#!/bin/sh
# FNVR Stereo: the game's openvr_api.dll (32-bit, OpenVR passed to a separate process) and FNVR-VRHost.exe (64-bit)
set -e
cd "$(dirname "$0")"
TC=${TC:-/home/claude/tools/llvm-mingw-20250528-ucrt-ubuntu-22.04-x86_64/bin}
OPENVR=${OPENVR:-/home/claude/valvesoftware/openvr}   # (a checkout of https://github.com/ValveSoftware/openvr, tag v2.15.6)
mkdir -p out/Stereo/x64
$TC/i686-w64-mingw32-gcc -O2 -Wall -Wextra -Wno-unused-label -shared -static -s -Wl,--kill-at -o out/Stereo/openvr_api.dll vrproxy.c -ld3d11 -ldxgi -ldxguid -luuid
$TC/x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -mwindows -static -s -o out/Stereo/x64/FNVR-VRHost.exe vrhost.c -ld3d11 -ldxgi -ldxguid -luuid
cp "$OPENVR/bin/win64/openvr_api.dll" out/Stereo/x64/openvr_api.dll
$TC/i686-w64-mingw32-gcc -O2 -Wall -Wextra -shared -static -s -Wl,--kill-at -o out/FNVRStereo.dll fnvrstereo.c
