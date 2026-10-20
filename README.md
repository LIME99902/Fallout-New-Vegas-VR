# FNVVRMC: Fallout: New Vegas VR with Motion Controls

FNVVRMC (the plugin file is `FNVR.dll`, and its settings are in `FNVR.ini`) is an [xNVSE](https://github.com/xNVSE/NVSE) plugin that plays **Fallout: New Vegas in a VR headset with full motion controls**. It runs through SteamVR and was built and tested on a **Meta Quest 3** connected to a PC.

You aim with your hand. You swing melee weapons and punches for real. You pick things up, throw them and climb. Guns go in holsters on your body, and you read the Pip-Boy on your actual wrist while the world keeps moving. On top of all that there is a Dying Light-style grappling hook.

> Current version: **2.0.4**, **[download it from the Releases page](https://github.com/LIME99902/Fallout-New-Vegas-VR/releases/latest)** (`FNVVRMC-2.0.4-full.zip`). This is a one-person hobby project made for the author's own setup (see the [mod list](docs/MODLIST.md)). It works well there, but other setups are untested.

- **[Motion controls guide](docs/CONTROLS.md)**: every button, gesture and keyboard key.
- **[Mod list](docs/MODLIST.md)**: the full load order and plugins FNVR is played with.
- **[Features](#features)**: what FNVR does, below.

---

## Requirements

- Fallout: New Vegas, PC (Steam version), with all DLC.
- [xNVSE](https://github.com/xNVSE/NVSE) 6.4 or newer.
- SteamVR and a PC VR headset. Tested on a Quest 3 (Link, Air Link or Virtual Desktop into SteamVR).
- Touch-style motion controllers (two grips, two triggers, A/B/X/Y, two thumbsticks).
- [JIP LN NVSE](https://www.nexusmods.com/newvegas/mods/58277) (scopes, climbing, holstered weapons and the quick-loot window use it).
- Optional:
  - **Just Assorted Mods** for the quick-loot window features.
  - **VATS Alternative - Bullet Time** for the left-trigger bullet time.
  - **New Vegas True Scopes** for the scope features.

## Install

1. Install xNVSE and JIP LN NVSE, and make sure the game starts through it.
2. Download **`FNVVRMC-2.0.4-full.zip`** from the **[Releases page](https://github.com/LIME99902/Fallout-New-Vegas-VR/releases/latest)** and unzip it into your `Fallout New Vegas` folder. It puts `FNVR.dll` in `Data\NVSE\Plugins\`.
3. The zip also includes a starter `Data\NVSE\Plugins\FNVR\FNVR.ini`. FNVR fills in everything else with defaults.
4. Start SteamVR, put the headset on and launch the game through xNVSE.
5. In game:
   1. Hold **both thumbsticks in for a second** to open the VR settings panel.
   2. Run **Body & profiles → Calibrate my body**.
   3. Point your right controller at the centre of your view and **click both thumbsticks** to set your aim.

FNVR writes a log to `Fallout New Vegas\FNVR.log`. That's the first thing to look at if something doesn't work.

---

## Features

### Seeing the game
- The game is shown in the headset with head tracking, and the field of view widens to fill the headset (F9 toggles this).
- **Body matching:** your eyes sit at your character's eye height and your real floor is the game's floor. Calibrate once per person.
- **Profiles:** up to four players, each with their own height, arm reach, handedness, aim angle and world size.
- **Skip the intro:** on a new game, hold the right grip for 6 seconds to skip the intro movie. A ring in front of you fills up while you hold.
- **Main menu screen:** the main menu, loading screens and the pause menu appear on a big curved screen in front of you.
- **Live menus:** talking, trading, containers and the companion wheel don't freeze the game. The world keeps running around you, the menu appears in front of you, and turning your head away closes it.
- **HUD and menu size:** the HUD, pop-up menus and dialogue each have their own size setting, so they sit where you can see them instead of out at the edges. Menus can also be raised or lowered to eye level.
- **HUD parts:** health, AP, sneak indicator, compass, ammo/condition and crosshair can each be hidden, or the whole HUD switched off.
- **Trading:** in the vendor menu, left X accepts the deal (and takes everything in a container).
- **Menu pointer revolver:** in menus, a revolver in your right hand points at the screen and the trigger selects. Each click can fire a Ranger Sequoia shot (sound, recoil, flash), with its own volume setting.
- Optional real 3D stereo (Ctrl+F10). It's off by default because it doubles the drawing work.

### Your body and hands
- **Hand aiming:** guns shoot where your hand points, and your head just looks around. Calibrate once by clicking both sticks, then fine-tune with F3/F4/F6/F7.
- **Two-handed aiming:** put your off hand on the gun and squeeze its grip to steady it. Works for two-handed melee weapons too (sledgehammers, axes).
- **Real arms:** the in-game arms follow your controllers, the elbow follows your hand's angle, and the arms can be sized to yours.
- **Finger tracking:** fingers follow the controller's touch sensors.
- **Real crouching and leaning:** crouch to reach the floor, lean over tables, and your body follows once you lean far enough.
- **Melee by swinging:** with fists or a melee weapon, a real swing of your hand attacks. Optionally the right trigger attacks too. Automatic melee weapons (chainsaw, Ripper, Industrial Hand) run while the trigger is held.
- **Melee angle:** each melee weapon's angle in your hand can be set (tilt, turn, roll) so it sits straight.

### Objects and the world
- **Pick things up:** with no weapon out, squeeze a grip next to a loose object to hold it in that hand. Fingers wrap around it.
- **Throwing:** let go while your hand is moving to throw. Thrown objects hurt what they hit, scaled by speed, weight and your Melee/Unarmed skills. An option protects your companions.
- **Physical hands:** your hands and weapons knock loose objects around and stop at walls and tables. Held objects can't lift you off the ground.
- **Climbing:** grab ledges and walls with the grips. Your body below you is hidden while you climb, and climbing is off while you're sitting, so the grip stands you up.

### Holsters and gestures
- **Holsters:** right grip at your right hip draws or holsters pistols and one-handed melee weapons. Right grip over your right shoulder does the same for long guns and two-handed melee weapons.
- **Visible holsters:** a holstered weapon is shown on your body: a pistol on your hip, a rifle over your shoulder. Its angle can be adjusted per holster.
- **Chest grab:** squeeze a grip at the middle of your chest to open or close the pause menu.

### Pip-Boy on your wrist
- **Opening it:** raise your left wrist to your face and the Pip-Boy opens on your actual arm. Lower it and it closes. It opens instantly, without the game's slow raise animation.
- **Live world:** the world can keep running while the Pip-Boy is open, like Fallout 4 VR (F10). The menu shows the same way whether the world is paused or running.
- **Menu display:** FNVR copies the Pip-Boy menu from the game every frame and draws it on the Pip-Boy screen on your arm.
- **Placement:** slide, size and tilt the menu on your arm with the controllers. Once saved, it stays put.
- **Black boxes:** both dark backgrounds (the Pip-Boy screen box and the menu background) can be switched on or off separately.
- **Forearm roll:** the forearm turns naturally with your wrist, with a real-arm limit so it never flips over.
- **Map:** the right stick moves the map cursor (at reduced strength, so the running world barely turns). A held button never carries over when the Pip-Boy closes, so fast travel won't make you jump.

### Weapons
- **See-through scopes:** look through a scope on your gun for a magnified view. Raising a scoped gun to your eye brings up the game's scope view. (Works with New Vegas True Scopes.)
- **Scope placement per gun:** the picture's height (in cm) and size can be set for each gun model; guns sharing a model share the setting. Models without a lens part start at the gun's highest point.
- **Fire rate:** a fire rate setting (25%-400%) for the gun in your hand.
- **Fast revolvers:** revolvers (Ranger Sequoia, Lucky, That Gun, .44, Schofield, S&W 500...) fire as fast as you pull the trigger.
- **Ranger Sequoia:** set back to its 5-round cylinder if another mod changed it.
- **Aim down sights:** optional left-trigger aim down sights.
- **Bullet time:** the left trigger can press the bullet-time key for VATS Alternative - Bullet Time.

### Movement
- **Sprint:** click and hold the left stick. Running jumps keep their speed in the air.
- **Arm-swing running:** pump your arms like you're running, Blade & Sorcery style, to move where you look. Swing hard to sprint. The stick still works.
- **Grappling hook (Dying Light style):** squeeze grip, then pull the trigger, and a line fires where your hand points and pulls you there, picking up speed as it goes. A trigger pulled on its own stays bullet time.
  - Let go early to launch yourself.
  - Fire the other hand mid-pull to swing on.
  - Optional: a rope you can see, hooking onto people and creatures, controller rumble, and no fall damage after a launch.
  - Range, speed, launch strength, hop and air drag can all be changed in the settings.
  - Works with either hand. The right hand only grapples while no gun is out.
  - Your body below you is hidden while you grapple, so it never ends up in front of you.
- **No fall damage in power armor** (optional).

### Kid-friendly options
Three separate switches:
- Bloody Mess bursting bodies (you keep the perk's extra damage).
- Dismemberment.
- All blood effects and blood decals.

### Quick loot (with Just Assorted Mods' loot menu)
- The quick-loot window is moved from the far right edge to the middle of your view, with an optional sideways nudge.
- Controller mapping: right A takes the item, right B takes everything, the right stick scrolls, and right grip (or left Y) opens the full container. The right stick never turns you while it's up.
  - The loot keys are sent as keyboard presses, so the game window needs to be the active window on your PC.

### Performance
- Bullet hits leave no wall decals; the impact smoke stays. This removes the hitch when shots hit walls. These are `Fallout.ini` and `FalloutPrefs.ini` tweaks listed in the [mod list](docs/MODLIST.md#ini-tweaks).
- A background frame copy keeps the headset smooth.

---

## The VR settings panel

Hold **both thumbsticks in for about a second** (or press **F11**) and a settings panel appears in front of you.
- Point at a line with your right hand and pull the trigger, or use either stick: up and down to choose, left and right to change.
- **B** goes back.
- Every change is saved to `FNVR.ini` straight away.

Pages:
- Body & profiles
- Pip-Boy
- Controls
- Holsters & gestures
- Hands & objects
- Display & menus
- HUD & menus
- Weapons & scopes
- Movement & comfort
- Kid friendly stuff

## Building from source

The whole plugin is one C file, `src/fnvr.c`, built as a 32-bit Windows DLL with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw):

```sh
TC=/path/to/llvm-mingw/bin ./build.sh   # -> out/FNVR.dll
```

`src/openvr_capi.h` is Valve's OpenVR C API header (BSD 3-Clause, © Valve Corporation). `src/nvse_api.h` is a minimal subset of the xNVSE plugin interface.

## Credits

- Fallout: New Vegas © Bethesda Softworks / Obsidian Entertainment. This project is not affiliated with them.
- xNVSE team, JIP LN NVSE, and every mod author in the [mod list](docs/MODLIST.md).
- OpenVR © Valve Corporation.
