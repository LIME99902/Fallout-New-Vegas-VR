"""Builds 'FNVVRMC - Cyberware.esp': the forms FNVR's cyberware fires and casts - the arm rocket launcher's mini-rockets and
their explosions (frag, thermal, electric, toxic, shrapnel) and the effects arm blades / Mech Arm fists put on what they hit.
Master: FalloutNV.esm only. usage: make_cyber.py FalloutNV.esm out.esp"""
import sys, struct
sys.path.insert(0, '/home/claude/hunter')
from tes import *

FNV = sys.argv[1]; OUT = sys.argv[2]
_, it = read_plugin(FNV, want={'EXPL', 'PROJ', 'ENCH', 'MGEF'})
R = {r.fid: r for r in walk(it)}
nid = [0x01000800]
def new(): nid[0] += 1; return nid[0] - 1
def fid(v): return struct.pack('<I', v)

FIRE, SHOCK, POISON, DMGHP, PULSE, FLAMERVIS = 0x14B, 0x14D, 0x31851, 0x33595, 0x5B146, 0x44AF7

def ench(edid, name, effects):
    r = Rec('ENCH', new(), subs=[['EDID', z(edid)], ['FULL', z(name)], ['ENIT', struct.pack('<IIIB3s', 2, 0, 0, 0, b'\0\0\0')]])
    for mg, mag, dur in effects:   # (magnitude, area 0, duration, on target, actor value from the effect itself)
        r.subs += [['EFID', fid(mg)], ['EFIT', struct.pack('<IIIIi', mag, 0, dur, 2, -1)]]
    return r

# what blades and Mech Arm fists put on whoever they hit (cast on them by FNVR)
E_THERMAL = ench('FNVVRMCEnchThermal', 'Thermal', [(FIRE, 4, 5), (FLAMERVIS, 0, 5)])
E_SHOCK = ench('FNVVRMCEnchElectric', 'Electric', [(SHOCK, 15, 0)])
E_TOXIC = ench('FNVVRMCEnchToxic', 'Toxic', [(POISON, 3, 10)])
E_BLEED = ench('FNVVRMCEnchBleed', 'Bleeding', [(DMGHP, 3, 8)])
# what the launcher's explosions put on everyone they catch
X_SHOCK = ench('FNVVRMCEnchBlastElectric', 'Electric Blast', [(SHOCK, 20, 0), (PULSE, 0, 6)])
X_TOXIC = ench('FNVVRMCEnchBlastToxic', 'Toxic Cloud', [(POISON, 5, 10)])
X_BLEED = ench('FNVVRMCEnchBlastShrapnel', 'Shrapnel', [(DMGHP, 4, 10)])
X_FIRE = 0x14DDDC   # (the incendiary grenade's: burning for 15 seconds, flames on them)
enchs = [E_THERMAL, E_SHOCK, E_TOXIC, E_BLEED, X_SHOCK, X_TOXIC, X_BLEED]

def expl(edid, base, ench_id):
    r = R[base].copy(); r.fid = new(); r.subs = [x for x in r.subs if x[0] != 'MODT']
    r.set('EDID', z(edid))
    d = bytearray(r.get('DATA')); struct.pack_into('<3f', d, 0, 250.0, 70.0, 320.0)   # force, damage (FNVR sets it per shot), radius
    r.set('DATA', bytes(d))
    r.subs = [x for x in r.subs if x[0] != 'EITM']
    if ench_id:
        i = [k for k, x in enumerate(r.subs) if x[0] == 'MODL'][0] + 1
        r.subs.insert(i, ['EITM', fid(ench_id)])
    return r

kinds = [('Frag', 0x5F9CD, 0), ('Thermal', 0x14DDE3, X_FIRE), ('Electric', 0x298A9, X_SHOCK.fid), ('Toxic', 0x12701D, X_TOXIC.fid), ('Shrapnel', 0x179DA, X_BLEED.fid)]
expls, projs = [], []
for name, base, en in kinds:
    x = expl('FNVVRMCArmRocketExplosion' + name, base, en); expls.append(x)
    p = R[0x1BB6].copy(); p.fid = new(); p.subs = [y for y in p.subs if y[0] != 'MODT']
    p.set('EDID', z('FNVVRMCArmRocket' + name)); p.set('FULL', z('Mini-Rocket'))
    d = bytearray(p.get('DATA')); struct.pack_into('<I', d, 36, x.fid); p.set('DATA', bytes(d))
    projs.append(p)

top = [Grup(b'ENCH', 0, enchs), Grup(b'EXPL', 0, expls), Grup(b'PROJ', 0, projs)]
def count(items): return sum(1 + (count(x.items) if isinstance(x, Grup) else 0) for x in items)
hdr = Rec('TES4', 0, subs=[['HEDR', struct.pack('<fiI', 1.34, count(top), nid[0])], ['CNAM', z('FNVVRMC')],
                           ['SNAM', z('FNVVRMC cyberware: arm launcher rockets and the effects of arm weapons.')],
                           ['MAST', z('FalloutNV.esm')], ['DATA', bytes(8)]])
open(OUT, 'wb').write(hdr.pack() + b''.join(g.pack() for g in top))
print('wrote', OUT, count(top), 'records')
