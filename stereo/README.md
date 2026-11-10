# FNVR Stereo

A separate part of FNVVRMC for the headset picture. Stage 1 (this): **OpenVR runs in its own 64-bit process.**

- `openvr_api.dll` (32-bit, goes in `Data\NVSE\Plugins\FNVR\Stereo\`) replaces Valve's library for FNVR. It starts
  `x64\FNVR-VRHost.exe` and passes every OpenVR call FNVR makes to it through shared memory.
- `x64\FNVR-VRHost.exe` + Valve's 64-bit `x64\openvr_api.dll` connect to SteamVR, so SteamVR's client (vrclient, its
  threads and buffers) is no longer inside the game's 4 GB address space.
- Pictures cross as shared D3D11 textures: the GPU copies them, nothing goes through the CPU between the processes.
- FNVR uses it automatically when the files are there (`[Stereo] bOutOfProcessVR=1`, the default). If the host can't
  start, FNVR falls back to OpenVR inside the game. Logs: `FNVR_Stereo.log` (game folder) and
  `Data\NVSE\Plugins\FNVR\Stereo\x64\FNVR-VRHost.log`.

Stage 2 - **zero copy (Direct3D 9Ex shared textures)**:
- `FNVRStereo.dll` (an xNVSE plugin, goes in `Data\NVSE\Plugins\`) makes the game's Direct3D 9 device a Direct3D 9Ex one.
  D3D9Ex doesn't allow the "managed" pool New Vegas loads nearly every texture and buffer into, so each of those is made
  in the default pool as a dynamic (lockable) resource instead. Side effect: they no longer keep a second copy in the
  game's own memory (like ENBoost).
- FNVR then scales each eye picture straight into a shared texture; the VR host opens it on its own D3D11 device and
  hands it to SteamVR. The GPU -> CPU -> GPU copy is gone.
- Needs Windows' own `d3d9.dll`: with DXVK's / ENB's `d3d9.dll` in the game folder it stays off (logged).
- Off: `[Stereo] bD3D9Ex=0` (the plugin) or `[Stereo] bZeroCopy=0` (FNVR keeps the CPU copy). Log: `FNVRStereo.log`.

Stage 3 - **fast alternate eyes**:
- With alternate-eye stereo, the headset now waits once per *pair* of eyes: the left-eye frame runs straight on (its head
  pose predicted for the refresh the pair will show at), the right-eye frame waits and the pair is submitted together -
  90 pairs a second at 90 Hz instead of 45. `[Stereo] bFastAlternate=1` (default).
- The game has to draw 180+ frames a second for that. New Vegas Tick Fix treats any frame faster than its
  `iMaxFPSTolerance` (59 by default) as that long, which would put the game in fast motion; FNVR raises it to 240 in
  `NVTF.ini` once (`[Stereo] bAdjustNVTF`), and the game needs one restart.

Notes:
- **D3D9Ex shared textures**: the game's own Direct3D 9 device becomes D3D9Ex so each eye picture goes straight to the
  host as a shared texture - removing the GPU -> CPU -> GPU copy FNVR does today. Needs plain D3D9 (no DXVK / ENB
  `d3d9.dll`).
- **Alternating-eye stereo at 90+ pairs per second**, with the engine's 10 ms minimum frame time removed.

Build: `sh stereo/build.sh` (llvm-mingw, i686 + x86_64).
