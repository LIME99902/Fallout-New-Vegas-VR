# One-click install: Nexus Collections

Players install FNVVRMC and everything it needs with **one click** through a Nexus **Collection**:
Vortex downloads every mod, installs it, and sets the load order. Free Nexus accounts click "download"
once per mod in a queue, and Premium accounts are fully automatic.

Two collections, so people can choose:

| Collection | What it is | Size |
|---|---|---|
| **FNVVRMC - VR Essentials** | FNVVRMC plus what it needs and the stability fixes. Plays the vanilla game in VR. **Point most people here.** | ~20 mods |
| **FNVVRMC - LIME's VR Playthrough** | The full setup used to build and test the mod (weapons, companions, Long 15, visuals, sounds). | ~150 mods |

---

## 1. VR Essentials: the list

Nexus mod IDs are taken from the author's own working install.

### Required
| Mod | Nexus ID | Why |
|---|---|---|
| **FNVVRMC** | *(your mod page)* | The VR mod |
| xNVSE | 67883 | Script extender: FNVVRMC is an xNVSE plugin |
| JIP LN NVSE Plugin | 58277 | Scopes, climbing, holsters, quick-loot, ammo commands |
| JohnnyGuitar NVSE | 66927 | Script functions the mod uses |
| ShowOff xNVSE | 72541 | Script functions the mod uses |
| kNVSE | 71336 | Animation fixes the other plugins expect |
| FNV 4GB Patch | 62552 | **Stops the out-of-memory crashes** (players must run the patcher once, see below) |
| NVTF - New Vegas Tick Fix | 66537 | Smooth frame pacing, which is essential in a headset |
| NVTF - INI | 66537 (INI file) | Its settings |
| Crash Logger | 72317 | So crash reports can be read |

### Strongly recommended
| Mod | Nexus ID | Why |
|---|---|---|
| YUP - Base Game and All DLC | 51664 | Bug fixes |
| YUPDate (+ Config) | 90824 | YUP updates |
| Unofficial Patch NVSE Plus | 71239 | Engine/script fixes |
| Engine Optimizations | 80993 | Performance |
| Fast Weapon Lag Fix | 71973 | Removes a stutter |
| ActorCause Save Bloat Fix | 80666 | Save stability |
| Just Assorted Mods | 66666 | Quick-loot window features in FNVVRMC |
| VATS Alternative - Bullet Time | 62278 | Bullet time (grip + trigger) |
| New Vegas True Scopes | 74824 | Scope features in FNVVRMC |
| The Mod Configuration Menu | 42507 | Settings menus for other mods |

### Leave OUT of both collections
| Mod | Why |
|---|---|
| Enhanced Camera | Moves the same first-person camera and body FNVVRMC controls |
| ENB / DXVK `d3d9.dll` | Not on Nexus, and changes the renderer FNVVRMC hooks; add later only if you know your setup |
| `VC_redist.arm64`, `dxwebsetup` | Installers dragged into Vortex by accident, not mods |
| FNV4GB **for Linux** | Windows players need the regular FNV 4GB Patch |

---

## 2. Making the collections in Vortex (one time, ~20 minutes)

FNVVRMC must be **on Nexus and installed through Vortex** to go in a collection. So first upload the
latest FNVVRMC zip to your Nexus page. Then in Vortex: *Mods → Install from file*, or the
"Mod Manager Download" button on your page. Remove the hand-copied FNVR.dll first.

**VR Essentials:**
1. Vortex → **Profiles** → **Add profile** → name it "VR Essentials". In it, enable only the mods in section 1.
2. Vortex → **Collections** → **Create a collection** → **From profile** → "VR Essentials".
3. In the collection editor:
   - **Mods** tab: set everything to *Required*. Set the "Strongly recommended" mods to *Recommended* if you
     want people to be able to untick them.
   - **Mod rules / load order**: Vortex records the plugin order automatically. Check that FNVVRMC is in it.
   - **Instructions**: paste the text from section 3. Use the per-mod note on FNV 4GB Patch: *"After installing, run FNVpatch.exe once
     from the game folder."*
4. **Upload**, then on the Nexus collection page add a description and screenshots, and **Publish**.

**LIME's VR Playthrough:** do the same from your normal profile, leaving out the mods in "Leave OUT".

**Updating:** when you change the mod or the list, open the collection in Vortex, then *Upload update*.
Players get a "new revision" button.

---

## 3. Text for the collection page (paste-ready)

> **FNVVRMC - VR Essentials**
>
> Everything you need to play Fallout: New Vegas in VR with full motion controls, in one click.
>
> **Before you start**
> - Fallout: New Vegas on **Steam**, with all DLC, started once normally.
> - **Vortex** (latest) and **SteamVR** with your headset working (Quest 3 via Link / Air Link / Virtual Desktop, Index, etc.).
>
> **Install**
> 1. Click **Add to Vortex** and let it finish. Free Nexus accounts: click download in each pop-up.
> 2. Run **FNVpatch.exe** in your Fallout New Vegas folder **once** (the 4GB patch).
> 3. Start **SteamVR**, then launch the game from **Steam** as usual.
> 4. The first time, FNVVRMC asks for your **headset, wired or wireless, and how powerful your PC is**. Pick them, then
>    **restart the game once**, and it sets the resolution for you.
> 5. Calibrate your height and arms when asked, and play.
>
> **Problems?** Send `FNVR.log` from your Fallout New Vegas folder.

---

## 4. For people who don't use Vortex

Point them to the manual steps in the main README. The release zip already contains FNVVRMC, its OpenVR library and
default settings. They add xNVSE, JIP LN, JohnnyGuitar, ShowOff, kNVSE, NVTF and the 4GB Patch themselves.
