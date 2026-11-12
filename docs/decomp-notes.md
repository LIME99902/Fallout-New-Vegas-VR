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

## HUD parts on their own panels (2.7.0)
- Tile (xNVSE GameTiles.h): +0x04 child list (node: +0 next, +8 tile), +0x14/+0x18 values (each `{id, tile, float}`),
  +0x20 name, +0x28 parent, **+0x2C NiNode**. Trait ids: 0xFA1 x, 0xFA2 y, 0xFA3 visible, 0xFA6 clipwindow, 0xFA8 locus,
  0xFA9 alpha, **0xFB0 height, 0xFB1 width**.
- How a tile is placed (Tile update, change flag 1, in the decomp's tile_ob1_variant.cpp): with *locus* set (or as a
  child of the interface root) its node's local translation is `(x, depth, -y)` (root children also `-screenW/2`,
  `+screenH/2`), so the node sits at its top-left corner. Without locus the node stays at its parent's spot and the
  shapes themselves are moved by `(x, -y)`. Children tiles hang from their parent tile's node. So a piece's absolute
  position = its own x/y + the x/y of every ancestor with locus (`@fnv 0x00a013d0` / `0x00a01440` add them up).
- A clip window (0xFA6) cuts its children to its own box (x, y, width, height through the locus chain), as a scissor
  rectangle - the compass strip is longer than what you see, so its box is used, not its shapes.
- Interface camera: `InterfaceManager (0x011D8A80) + 4` scene graph, `+0xAC` its NiCamera: world rotate columns =
  forward / up / right, frustum at +0xDC (l, r, t, b), +0xF4 ortho. FNVR widens that frustum for HUD size, so a node's
  world position through this camera gives where it lands in the picture: `u = (dot(P - C, right) - l) / (r - l)`,
  `v = (t - dot(P - C, up)) / (t - b)`.
- NiObject vtable +0x08 = GetRTTI (`{name, parent}` records: "NiTriShape" -> "NiTriBasedGeom" -> "NiGeometry"), +0x0C
  GetAsNiNode. NiGeometry +0xB8 geometry data: +0x08 vertex count, +0x20 vertices (model space).
- FNVR measures a part from the corners of its visible shapes (culled nodes, visible = 0, alpha = 0 left out; clip
  windows as their box), copies that rectangle of the interface picture onto the part's own SteamVR overlay, then clears
  it from the picture before the curved HUD gets it.

## The gun hand's model: first-person object vs world model (2.7.2)
- TESObjectWEAP +0x250..+0x26C: eight first-person objects (TESObjectSTAT*: no mods, mod 1, 2, 3, 1+2, 1+3, 2+3, 1+2+3;
  `@fnv 0x004ab500` picks one by the mod bits). The STAT's model path is at STAT +0x34.
- `BipedAnim::SetWeaponModel` (`@fnv 0x004ab400`): starts from the weapon's own (world) model for its mods, and only for
  the player's two bodies (first and third person) swaps in the first-person object when there is one. With those
  pointers cleared the player's hand gets the world model - which is all FNVR's "World model in your hand" does.
- BipedAnim: `object[20]` at +0x2C, 16 bytes each `{pParent form, m_pModel, m_pSceneObject, bHasAnimation}`; the weapon
  is slot 5 (+0x7C). Player +0x68C = first-person BipedAnim, Character +0x1B4 = third-person.
- `Actor::AttachWeapon(TESObjectWEAP*)` (`@fnv 0x00571760`, thiscall): attaches the weapon to both of the player's
  bodies through `BipedAnim::AttachBipedWeapon` (`@fnv 0x004ab750`), which skips a slot already holding that weapon with
  a drawn part. Clearing the slot's pParent first makes it rebuild: `SetWeaponModel` then removes the old part itself
  (`RemovePart` `@fnv 0x004aaff0` -> `ClearBipedObject` `@fnv 0x004ab020`, which releases the old model file).
- The script command EquipItem does nothing for an item that's already worn (`Cmd_EquipItem_Execute` checks the
  ExtraWorn flag first), so "EquipItem the equipped weapon" never rebuilds it.
