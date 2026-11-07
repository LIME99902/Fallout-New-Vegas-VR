# Manual reloading (2.3.0)

Every gun is reloaded by hand. You have **an ammo pouch on each side of the front of your belt** - you can see them when you look down while a gun is out (turn them off: *Weapons → Reload: ammo pouches you can see*). They stay with your body, not your head, so looking down at them doesn't move them.
Either hand can take from either pocket (turn the second one off: *Holsters & gestures → Ammo pockets on both hips*). Your controller buzzes lightly when your hand is in it and there's something to take.

Turn it off in the VR settings: **Weapons → Manual reloading** (`[Reload] bManualReload=0`). The game then reloads for you as before.
The pocket can be moved under **Holsters & gestures → Ammo pocket** (below eyes / to the side / in front / size).

| Gun | How to reload |
|---|---|
| **Magazine guns** (pistols, SMGs, rifles) | **B** drops the magazine (**Y** for the left-hand gun) - or **grab the magazine** with your free hand (grip on it) and pull it out; let go and it drops. Reach into a pouch with your free hand, **squeeze the grip** to take a magazine, keep holding it and **push it into the gun** where the magazine goes. If you shot the gun empty, **grab the bolt / slide** with your free hand (grip near the gun), **pull it back** about 6 cm and **let go** - it runs forward and chambers a round. |
| **Energy weapons** (cells) | Same as magazines, no slide to pull. |
| **Heavy weapons** (minigun, flamer, launchers with a magazine) | Same, with an ammo box or pack - touch it anywhere to the gun. |
| **Revolvers** | **B** opens the cylinder (it slides out to the right). Take a **speedloader** from a pouch, push it into the cylinder, then **flick the gun to the right** to snap it shut. |
| **Tube shotguns, lever-action rifles** | Take a **shell** from the pocket and push it into the loading port under the gun - one at a time. If it was empty, **pump / work the lever** afterwards (pull back with your free hand, or press **B**). |
| **Break-action shotguns** (Caravan, sawed-off) | **B** breaks it open. Take shells from the pocket, push them into the breech. **B** snaps it shut. |
| **Single-shot weapons** | Take a round from the pocket and push it into the breech. |

**Dual wielding** (a gun in each hand, so neither hand is free): drop the magazine / open the gun as usual, then **touch the gun itself to your left pocket** - it's reloaded (and closed, and racked).

A round stays in the chamber when you drop a magazine from a gun that still had rounds in it, so you can still fire that one shot.
A half-full magazine you drop isn't lost: its rounds stay in your inventory.

## How it decides what kind a gun is
From the weapon's type, its name and its model: a part called *cylinder* or a name like *revolver / magnum / Sequoia* is a revolver;
*shotgun / lever / repeater / brush gun / trail carbine...* loads shell by shell (except the Riot Shotgun's drum); *sawed-off / Caravan /
double-barrel* is a break-action; one round per load is single-shot; energy weapons use cells; miniguns, flamers and launchers use boxes.
The log says what it picked: `Reload: gun hand's gun <name> - <kind> reload, <n> rounds`.

If a gun gets the wrong kind, set it in `FNVR.ini`:

```ini
[ReloadTypes]
; the weapon's form ID (8 hex digits) = mag | cell | box | revolver | tube | break | single
000E3778=revolver
```

## Test checklist (one type at a time)
For each: does the step happen, does it feel right, and what does `FNVR.log` say (lines starting `Reload:`)?

1. **Magazine pistol** (9mm Pistol): B drops the mag (it falls out), pocket grab, push into the grip, fire. Then empty it fully: B, new mag, rack the slide.
2. **Magazine rifle / SMG** (Varmint Rifle, Service Rifle, 9mm SMG): same - is the spot where the mag goes in right?
3. **Energy pistol / rifle** (Laser Pistol, Laser Rifle): no rack needed.
4. **Revolver** (Ranger Sequoia, .357): B open, speedloader, B close.
5. **Tube shotgun** (Hunting Shotgun) and **lever-action** (Cowboy Repeater): shell by shell, pump when empty.
6. **Break-action** (Caravan Shotgun, Sawed-off): B open, shells, B close.
7. **Single-shot** (Grenade Rifle): one round in.
8. **Heavy** (Minigun, Flamer): box to the gun.
9. **Dual wield** (two revolvers): open/drop, gun to the pocket.
10. **Left-hand gun alone** (right hand empty): Y, right hand to the left pocket, into the left gun.

## What you see
- **Orange outlines** (like Titanfall 2 VR): while a gun waits for its slide, pump or lever, that part glows orange; while you carry
  a magazine, speedloader, shell or round to the gun, an orange ring marks where it goes in (a revolver's open cylinder and a
  break-action's barrels are outlined too). Off: *Weapons → Reload: orange outlines* (`[Reload] bOutlines=0`).
- **Magazines** drop out of the gun, and the new one sits in your free hand on its way in (when the gun's model has a magazine part).
- **Shells, speedloaders, rounds** show in your hand when the gun's model has such a part (many first-person models carry them for the
  game's reload animation). The log lists what each gun's model has: `Reload: gun hand's gun model parts - ...`.
- **Slides and bolts** lock back when the gun is empty with the magazine out, and follow your hand as you pull them. **Pumps** slide
  back and **levers** swing down as you work them. **Cylinders** swing out to the side while open; **break-action barrels** tip down.
- **Where things go in** is measured on each gun's own shape the first time you hold it: the magazine's own spot, the cylinder, the
  bottom of a pistol grip, the well in front of a rifle's trigger, a shotgun's loading port, the breech. The log says what it found:
  `Reload: ... where things go in measured on its shape (x m in front, y m up of the hand)`.

## Still limited
- A gun whose model has no shell / speedloader part shows nothing in your hand for those (you still feel and hear them).
- Moving parts only move when the model names them in a way FNVR recognises (slide, bolt, charging handle, pump, forend, lever, cylinder, barrel).
