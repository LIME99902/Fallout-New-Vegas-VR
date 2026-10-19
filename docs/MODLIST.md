# Mod list

This is the exact setup FNVR is developed and played on: Fallout: New Vegas (Steam) with all DLC, mods managed with Vortex, on a Meta Quest 3 through SteamVR.
Only **xNVSE** is strictly required by FNVR; everything else is the author's personal list. Mods marked ★ are used by an FNVR feature.

## Engine and patches

- **xNVSE 6.4.4**: script extender. ★ Required.
- **4GB Patcher**: lets the game use more memory.
- **ENBoost** (Large Texture Support - ENB Boost, `d3d9.dll`): memory and texture-size fixes.
- **Yukichigai Unofficial Patch (YUP)** and **YUPDate**: bug fixes.
- **Unofficial Patch NVSE Plus**: bug fixes.

## NVSE plugins

| Plugin | Version | Notes |
|---|---|---|
| **FNVR** | 1.9.9 | This mod |
| xNVSE ★ | 6.4.4 | Script extender - required by FNVR |
| JIP LN NVSE ★ | 57.30 | Needed for the quick-loot window |
| JohnnyGuitar NVSE | 5.20 |  |
| kNVSE | 37 | Animation framework |
| ShowOff NVSE | 1.71 |  |
| lStewieAl's Tweaks | 8.60 |  |
| New Vegas Tick Fix (NVTF) | 10.61 | Smoother frame timing |
| UI Organizer | 2.30 |  |
| MCM Extensions | 1 |  |
| OneTweak | 4 | Window/fullscreen handling |
| Engine Optimizations | 1.00 |  |
| Cell Offset Generator | 1.04 |  |
| Fast Weapon Lag Fix | 1.00 |  |
| VATS Lag Fix | 1.00 |  |
| Viewmodel Shake Fix | 1.20 |  |
| Fog Culling | 2 |  |
| HD Pip-Boy ★ | 1 | Higher-resolution Pip-Boy menu (FNVR reads the menu picture this makes) |
| High Res Bloom | 6 |  |
| High Res Local Maps | 12 |  |
| Climate Control | 0 |  |
| Book Menu Restored | 1 |  |
| Actor Cause Save Bloat Fix | 1 |  |
| MLF | 5 |  |
| Enhanced Camera ★ | 1 | First-person body |
| Cobb Crash Logger | 3 | Crash logs |

## Load order (plugins.txt)

| # | Plugin | Notes |
|---|---|---|
| 00 | FalloutNV.esm |  |
| 01 | DeadMoney.esm |  |
| 02 | HonestHearts.esm |  |
| 03 | OldWorldBlues.esm |  |
| 04 | LonesomeRoad.esm |  |
| 05 | GunRunnersArsenal.esm |  |
| 06 | ClassicPack.esm |  |
| 07 | MercenaryPack.esm |  |
| 08 | TribalPack.esm |  |
| 09 | CaravanPack.esm |  |
| 0A | YUP - Base Game + All DLC.esm |  |
| 0B | YUPDate.esm |  |
| 0C | Landscape Texture Improvements.esm |  |
| 0D | Landscape Texture Improvements - YUP Patch.esm |  |
| 0E | DLC Enhancements.esp |  |
| 0F | Unofficial Patch NVSE Plus.esp |  |
| 10 | Vanilla Enhancements.esm |  |
| 11 | TLD_Travelers.esm |  |
| 12 | NVMIM.esp |  |
| 13 | Functional Post Game Ending Lite.esm |  |
| 14 | Long15Reloaded.esm |  |
| 15 | New Vegas Redesigned II.esm |  |
| 16 | CompanionInfAmmo.esm |  |
| 17 | EZ_CompanionNVSE.esm |  |
| 18 | Walking_Inertia.esm |  |
| 19 | Ragdolls.esm |  |
| 1A | The Mod Configuration Menu.esp | MCM |
| 1B | JustAssortedMods.esp | ★ Quick-loot window (JLM) that FNVR centres and maps to the controllers |
| 1C | DelayDLCRedux.esp |  |
| 1D | Real Recoil.esp |  |
| 1E | NPCsSprint.esp |  |
| 1F | NewVegasTrueScopes.esp | ★ See-through scopes |
| 20 | DLC Followers All-In-One.esp |  |
| 21 | WeaponModsExpanded.esp |  |
| 22 | CCO - Ulysses Companion.esp |  |
| 23 | NCR Ranger Power Armor by JohnDev25 (Heavy Trooper Replacer).esp |  |
| 24 | NCR Ranger Power Armor by JohnDev25.esp |  |
| 25 | B - PAFrame.esp |  |
| 26 | new sequoia.esp |  |
| 27 | The Weapon Mod Menu.esp |  |
| 28 | EZ_VanillaFollowers.esp |  |
| 29 | 1nivCorgiWorld.esp |  |
| 2A | Windows of the Mojave v1.2.1.esp |  |
| 2B | Atmospheric Lighting Tweaks.esp |  |
| 2C | Classic Fallout Weapons Remastered v1.2.esp |  |
| 2D | 1nivCorgiComp.esp |  |
| 2E | CompanionInfAmmoOptional.esp |  |
| 2F | EZ_GLUE.esp |  |
| 30 | FrankHorriganBETA1.esp |  |
| 31 | GIAT_FAMAS.esp |  |
| 32 | Asurah_Reanimation_Handgrips.esp |  |
| 33 | Asurah_reanimation.esp |  |
| 34 | VeronicaDressFix.esp |  |
| 35 | AWO - MG42.esp |  |
| 36 | HitVector.esp |  |
| 37 | cowboys fan revolvers.esp |  |
| 38 | Handmade AMR.esp |  |
| 39 | BurnedManLives.esp |  |
| 3A | SWRevolvers.esp |  |
| 3B | companions.esp |  |
| 3C | Minimissilelauncher.esp |  |
| 3D | Kriss.esp |  |
| 3E | UnlimitedCompanions.esp |  |
| 3F | CompanionInfAmmoCheat.esp |  |
| 40 | Quickthrow.esp |  |
| 41 | Unique Recharger Rifle Base Game.esp |  |
| 42 | VM_Recoil.esp |  |
| 43 | IMPACT.esp | Bullet impact effects (FNVR's no-decal tweak keeps its smoke) |
| 44 | B42Inspect.esp |  |
| 45 | Player Combat Priority.esp |  |
| 46 | VATS Alternative - Bullet Time.esp | ★ Bullet time on the left trigger |
| 47 | Akimbo Lever.esp |  |
| 48 | AK_50.esp |  |
| 49 | Saiga12.esp |  |
| 4A | AimAssist.esp |  |
| 4B | P90.esp |  |
| 4C | GeneralLightingOverhaul.esp |  |
| 4D | Famine.esp |  |
| 4E | Famine - Flora.esp |  |
| 4F | Diagonal movement.esp |  |
| 50 | Bullet Tracers NV Shotgun.esp |  |
| 51 | AK47.esp |  |
| 52 | Classic MEC Gauss Minigun.esp |  |
| 53 | FNV Anim Set - Schofield Revolver.esp |  |
| 54 | FOVSlider.esp |  |
| 55 | ImmersiveRecoil.esp |  |
| 56 | Realistic Movement.esp |  |
| 57 | JSRS.esp |  |
| 58 | Malorian Arms 3516.esp |  |

## INI tweaks

These remove the short freeze when bullets hit walls (no bullet-hole decals; the impact smoke stays):

`Documents\My Games\FalloutNV\Fallout.ini`:
```ini
[General]
iPreloadSizeLimit=262144000

[Display]
iMaxDecalsPerFrame=0
bDecalsOnSkinnedGeometry=0
fDecalLifetime=10

[Decals]
bDecalOcclusionQuery=0
```

`Documents\My Games\FalloutNV\FalloutPrefs.ini`:
```ini
[Display]
iMaxDecalsPerFrame=0
iMaxSkinDecalsPerFrame=0
fDecalLOD1=1000
fDecalLOD2=1500
```
