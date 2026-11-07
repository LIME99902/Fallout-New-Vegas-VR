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

Planned next:
- **D3D9Ex shared textures**: the game's own Direct3D 9 device becomes D3D9Ex so each eye picture goes straight to the
  host as a shared texture - removing the GPU -> CPU -> GPU copy FNVR does today. Needs plain D3D9 (no DXVK / ENB
  `d3d9.dll`).
- **Alternating-eye stereo at 90+ pairs per second**, with the engine's 10 ms minimum frame time removed.

Build: `sh stereo/build.sh` (llvm-mingw, i686 + x86_64).
