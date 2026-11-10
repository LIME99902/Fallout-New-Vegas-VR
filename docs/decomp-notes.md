# Notes from fnv_decomp (github.com/momusdigital/fnv_decomp)

Matching decompilation of the Steam PC FalloutNV.exe 1.4.0.525 - the exact binary FNVR hooks.
No licence is granted for it: FNVR uses it only as a **reference** (to check offsets and understand
engine behaviour). No code is copied from it.

Caution: many of its type headers come from the Xbox 360 build's debug info, where Gamebryo
objects are bigger (e.g. NiAVObject 0xC0 there vs 0x9C on PC). Prefer offsets with a PC
`@fnv 0x...` function or an `offsetof(...) == ...` check behind them.

## TESObjectWEAP (size 0x388) - confirmed
| Offset | Field |
|---|---|
| 0x074 | TESEnchantableForm (object effect `EnchantmentItem*` at 0x078) |
| 0x0F4 | weapon type |
| 0x1C0 | criticalDamage (u16) |
| 0x1C4 | criticalPercent (float, multiplier) |
| 0x1C8 | critDamageFlags (bit: effect only on death) |
| 0x1CC | criticalEffect (`SpellItem*`) |
| 0x24C | impactDataSet (`BGSImpactDataSet*`) |

## How the game rolls a critical (HitData, @fnv 0x009b7060)
- chance = the attacker's crit chance for this weapon, through perk entry points 1 (attacker) and 0x24 (target);
  sneak attacks (target hasn't detected you) multiply it.
- roll `rand() % 1000 < chance * 10`.
- on a crit: damage += weapon criticalDamage (perk entry 2), HitData flag 4 set, flag 8 = the weapon's
  "on death" byte, and the HitData keeps the weapon's criticalEffect to apply.

FNVR's own blows: knockout = crit; otherwise player CritChance x criticalPercent, then
`target.CastImmediateOnSelf criticalEffect`.

## Skinning (for a mirrored left-hand power fist glove)
- NiGeometry (PC): properties at 0x9C, geometry data 0xB8, **NiSkinInstance* at 0xBC**.
- NiSkinInstance: +0x08 NiSkinData*, +0x0C NiSkinPartition*, +0x10 root parent, +0x14 `NiAVObject** bones`.
- NiSkinData: +0x0C skin transform (rot 3x3, trans, scale = 0x34), +0x40 `BoneData*`, +0x44 bone count.
  BoneData stride 0x4C: skin-to-bone transform (0x34), bound sphere (0x10), vertex weights*, count.
- The game binds skinned armour/weapon parts to a skeleton **by bone name** (BipedAnim, @fnv 0x004ade40):
  for each skin bone, find the node with the same name under the skeleton root and `SetBone(i, node)`.

Left glove plan: take the power fist's skinned glove, rebind each `Bip01 R ...` bone to the first-person
`Bip01 L ...` node, and mirror it. Left bones mirror right bones across their local Z, so premultiplying
each bone's skin-to-bone transform by F = diag(1,1,-1) on a private copy of the skin data gives the
mirror image. Triangle winding flips with a mirror, so the copy also needs its winding (or culling) reversed.
Needs the in-game MDL dump (FNVR.log "MDL" lines) first, to see which parts carry the skin.

What the dump showed (Saturnite Fist, 2.5.6 log): the first-person power fist has **no skinned parts** - it's a rigid
model (PowerFistRigid, pistons, sledge, hinges) hung from the skeleton's `Weapon` node under `Bip01 R ForeTwist`, and the
hand inside it is the player's own. So 2.5.11 mirrors the rigid model instead: its transform relative to `Bip01 R ForeTwist`
is measured while one is worn on the gun hand ([Arms] sFistOnForearm), and the other hand's copy hangs from
`Bip01 L ForeTwist` with local = F x that (F = diag(1,1,-1), a reflection - NiTransform keeps it, the renderer uses the
matrix as is). The reflection turns the faces round, so each piece gets a NiStencilProperty (made with the game's
@fnv 0x00a6f410) set to draw mode BOTH (flags 0x0C00) at NiGeometry+0xAC - NiD3DShader_SetupGeometryRenderStates
(@fnv 0x00be20e0) then turns culling off for it.
