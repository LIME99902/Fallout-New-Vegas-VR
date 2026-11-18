"""Builds 'FNVVRMC - Hunter.esp': Hunter, the ripperdoc at the New Vegas Medical Clinic."""
import sys, struct, pickle, re
sys.path.insert(0, '/home/claude/hunter')
from tes import *

UP = '/mnt/user-data/uploads/Downloads/fnvvrmc_hunter/'
OUT = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/hunter/out/FNVVRMC - Hunter.esp'

# ---------------------------------------------------------------- sources
small = pickle.load(open('/home/claude/hunter/data/fnv_small.pkl', 'rb'))
cells = pickle.load(open('/home/claude/hunter/data/fnv_cells.pkl', 'rb'))
_, lr = read_plugin(UP + 'LonesomeRoad.esm', want={'ARMO', 'DIAL'})
_, kr = read_plugin(UP + 'Kriss.esp')
FNV = {r.edid(): r for r in walk(small) if r.edid()}
LR = {r.edid(): r for r in walk(lr) if r.edid()}
KR = {r.edid(): r for r in walk(kr) if r.edid()}
cell = [r for r in walk(cells) if r.typ == 'CELL' and r.fid == 0xED19F][0]

# masters: 00 FalloutNV.esm, 01 LonesomeRoad.esm, 02 Kriss.esp ; our own forms 03xxxxxx
nid = [0x06000800]   # (6 masters: our own forms are 06xxxxxx; the low part - what saves keep - stays the same)
def new(): nid[0] += 1; return nid[0] - 1
def fid(v): return struct.pack('<I', v)

recs = {}   # top-level group label -> list of items

# ---------------------------------------------------------------- global the DLL watches
GLOB_ID = new()
glob = Rec('GLOB', GLOB_ID, subs=[['EDID', z('FNVVRMCHunterChrome')], ['FNAM', b's'], ['FLTV', f32(0.0)]])

# ---------------------------------------------------------------- black elite riot gear
def strip_hashes(r): r.subs = [s for s in r.subs if s[0] not in ('MODT', 'MO2T', 'MO3T', 'MO4T')]
ARM_ID, HLM_ID = new(), new()
arm = LR['NVDLC04ArmorRiotGearUnique'].copy(); arm.fid = ARM_ID; strip_hashes(arm)
arm.set('EDID', z('FNVVRMCHunterRiotGear')); arm.set('FULL', z("Hunter's Riot Gear"))
arm.set('MODL', z('NVDLC04\\Armor\\NVDLC04RiotGearArmor\\NVDLC04RiotGearArmorOutfitH.nif'))
arm.set('MOD2', z('NVDLC04\\Armor\\NVDLC04RiotGearArmor\\NVDLC04RiotGearArmorOutfit_H_GO.nif'))
arm.set('MOD3', z('NVDLC04\\Armor\\NVDLC04RiotGearArmor\\NVDLC04RiotGearArmorOutfitH_F.nif'))
hlm = LR['NVDLC04HelmetRiotGearUnique'].copy(); hlm.fid = HLM_ID; strip_hashes(hlm)
hlm.set('EDID', z('FNVVRMCHunterRiotHelmet')); hlm.set('FULL', z("Hunter's Riot Gear Helmet"))
hlm.set('MODL', z('NVDLC04\\Armor\\NVDLC04RiotGearArmor\\NVDLC04RiotGearArmorHelmetH.nif'))
hlm.set('MOD2', z('NVDLC04\\Armor\\NVDLC04RiotGearArmor\\NVDLC04RiotGearArmorHelmetH_GO.nif'))
for r in (arm, hlm): r.subs = [s for s in r.subs if s[0] not in ('MOD3', 'MOD4') or r is arm and s[0] == 'MOD3']

# (2.13.3) both pieces: +20 critical chance and 2 HP a second each, and light armor
def light_and_enchanted(r, ench_id):
    b = bytearray(r.get('BMDT')); b[4] &= ~(0x08 | 0x80) & 0xFF; r.set('BMDT', bytes(b))   # (general flags: 0x08 medium, 0x80 heavy)
    i_ = [k for k, x in enumerate(r.subs) if x[0] == 'FULL'][0] + 1
    r.subs = [x for x in r.subs if x[0] != 'EITM']
    r.subs.insert(i_, ['EITM', fid(ench_id)])
# ---------------------------------------------------------------- white/black Vector
VEC_ID = new()
vec = KR['WeapKrissCamo'].copy(); vec.fid = VEC_ID; strip_hashes(vec)
vec.subs = [s for s in vec.subs if not s[0].startswith('MO') or s[0] in ('MODL', 'MOD2')]
for s in vec.subs:   # Kriss.esp's own forms are 01xxxxxx inside it, 02xxxxxx here
    if len(s[1]) == 4 and s[0] not in ('DATA',) and s[1][3] == 0x01: s[1] = s[1][:3] + b'\x02'
vec.set('EDID', z('FNVVRMCHunterVector')); vec.set('FULL', z("Hunter's Vector"))
vec.set('MODL', z('weapons\\Kriss\\Kriss_hunter.nif'))
for i, n in ((1, 'acc'), (2, 'holo'), (3, 'acc_holo'), (4, 'supp'), (5, 'acc_supp'), (6, 'holo_supp'), (7, 'full')):
    vec.set('MWD%d' % i, z('weapons\\Kriss\\Kriss_%s_hunter.nif' % n))
# .45 ACP like a real Vector: Honest Hearts' .45 Auto (HonestHearts.esm is our 4th master, index 03)
AMMO = 0x0300947B                     # NVDLC02Ammo45Auto
vec.set('NAM0', fid(0x03009E70))       # NVDLC02AmmoList45Auto (all .45 Auto kinds)

# ---------------------------------------------------------------- Hunter
NPC_ID = new()
npc = FNV['CorporalFarber'].copy(); npc.fid = NPC_ID
npc.set('EDID', z('FNVVRMCHunter')); npc.set('FULL', z('Hunter'))
acbs = bytearray(npc.get('ACBS'))
flags = struct.unpack_from('<I', acbs, 0)[0] | 0x02 | 0x20          # essential, unique
flags &= ~0x80                                                   # fixed level
struct.pack_into('<I', acbs, 0, flags); struct.pack_into('<h', acbs, 8, 30)
npc.set('ACBS', bytes(acbs))
subs = []
for s, d in npc.subs:
    if s in ('CNTO', 'COED', 'PKID'): continue
    subs.append([s, d])
    if s == 'EAMT' or (s == 'RNAM' and not any(x[0] == 'EAMT' for x in npc.subs)):
        for item, n in ((ARM_ID, 1), (HLM_ID, 1), (VEC_ID, 1), (AMMO, 300)):
            subs.append(['CNTO', fid(item) + struct.pack('<i', n)])
    if s == 'AIDT':
        subs.append(['PKID', fid(FNV['DefaultSandboxEditorLocation256NoSleepOrWander'].fid)])
npc.subs = subs
npc.set('DATA', struct.pack('<I7B', 300, 6, 6, 10, 5, 6, 6, 6))
npc.set('NAM6', f32(1.17))   # a foot taller than an average man
skills = [40, 40, 50, 50, 45, 85, 60, 65, 75, 90, 55, 40, 60, 60]
npc.set('DNAM', bytes(skills) + bytes(14))

# ---------------------------------------------------------------- dialogue
QST_ID = new()
qst = Rec('QUST', QST_ID, subs=[['EDID', z('FNVVRMCHunterDialogue')], ['FULL', z('Hunter')],
                                 ['DATA', bytes([0x11, 60, 0, 0]) + f32(0.0)]])

def cond_is_hunter(): return struct.pack('<B3sfHHIIII', 0, b'\0\0\0', 1.0, 72, 0, NPC_ID, 0, 0, 0)

def split(text, lim=150):
    parts, cur = [], ''
    for snt in re.findall(r'[^.!?]+[.!?]+\s*', text.strip() + ' '):
        if cur and len(cur) + len(snt) > lim: parts.append(cur.strip()); cur = ''
        cur += snt
    if cur.strip(): parts.append(cur.strip())
    return parts

SCHR0 = bytes(20)[:18] + b'\x01\x00'
def info(text, prompt=None, choices=(), goodbye=False, script_set=False, greeting=False):
    r = Rec('INFO', new(), subs=[['DATA', bytes([0, 0, 0x01 if goodbye else 0, 0])], ['QSTI', fid(QST_ID)],
                                 ['PNAM', fid(0)]])
    for n, line in enumerate(split(text), 1):
        r.subs += [['TRDT', struct.pack('<IiI B3s I B3s', 0, 50, 0, n, b'\0\0\0', 0, 1, b'\0\0\0')],
                   ['NAM1', z(line)], ['NAM2', b'\0'], ['NAM3', b'\0']]
    r.subs.append(['CTDA', cond_is_hunter()])
    for c in choices: r.subs.append(['TCLT', fid(c)])
    if script_set:   # set FNVVRMCHunterChrome to 1  (compiled exactly like the game's own result scripts)
        r.subs += [['SCHR', struct.pack('<IIIIHH', 0, 1, 11, 0, 0, 1)], ['SCDA', bytes.fromhex('1500070047010002002031')],
                   ['SCTX', b'set FNVVRMCHunterChrome to 1'], ['SCRO', fid(GLOB_ID)]]
    else:
        r.subs.append(['SCHR', SCHR0])
    r.subs += [['NEXT', b''], ['SCHR', SCHR0]]
    if prompt: r.subs.append(['RNAM', z(prompt)])
    r.subs.append(['ANAM', fid(NPC_ID)])
    return r

T_CHROME, T_OPEN, T_BURNED, T_BYE = new(), new(), new(), new()
ALL = (T_CHROME, T_OPEN, T_BURNED, T_BYE)
LINES = {
    T_CHROME: ('FNVVRMCHunterTopicChrome', 'What kind of chrome do you have?', "Everything you need.", True, False),
    T_OPEN: ('FNVVRMCHunterTopicOpened', 'When did you open this place?',
             "After the first battle of Hoover Dam. I barely survived an encounter with the man they now call the Burned Man. "
             "They brought me here and patched me up, leaving me more machine than man. "
             "The docs are too busy to mess with chroming people, so I decided to fill their shoes. "
             "I just have to run the program and the autodoc does the hacking and slashing on you. "
             "Can't complain though. Without it I wouldn't be alive, and with it the NCR and I are much better off. "
             "Even the everyday locals have been chroming up lately, to be prepared for when the Legion comes. "
             "I've even heard the Legion might have a chromedoc of their own. "
             "Anyway, you look like you could use some more plating. Helps with the whole getting shot thing.", False, False),
    T_BURNED: ('FNVVRMCHunterTopicBurnedMan', 'You fought the Burned Man?',
               "Yeah. He was Caesar's right-hand man at the time. My Rangers were holding off the Legion, and when they rushed us, Graham and I got into it. "
               "Lost a lot of good men that day. Both sides just had bodies everywhere. "
               "Almost like leadership thought they weren't sending us there to die. "
               "Sorry, it's hard to go back to that day. Lost a lot of brothers for nothing, just so we could go and fight them another day. "
               "When they retrieved my body they realized I wasn't dead like the rest of First Company, so they brought me here, and the rest is history. "
               "I've been holding down the fort here against any Fiends or Legion that wander too far from home. "
               "Even the Fiends have chrome now. Jesus, what's next? Chromed-out Deathclaws?", False, False),
    T_BYE: ('FNVVRMCHunterTopicGoodbye', 'Goodbye.', "See you later, kid.", False, True),
}

dial_items = []
# GREETING: Lonesome Road's own copy of the topic (it already lists the base game's and LR's quests) + ours
greet = LR['GREETING'].copy(); greet.subs.append(['QSTI', fid(QST_ID)])
g_info = info("So you're the Courier I've heard so much about. Thought you'd be taller.", choices=ALL)
dial_items += [greet, Grup(0xC8, 7, [g_info])]
for t in ALL:
    edid, topic, text, scr, bye = LINES[t]
    d = Rec('DIAL', t, subs=[['EDID', z(edid)], ['QSTI', fid(QST_ID)], ['FULL', z(topic)], ['PNAM', f32(50.0)], ['DATA', b'\0\0']])
    i = info(text, prompt=topic, choices=() if bye else ALL, goodbye=bye, script_set=scr)
    dial_items += [d, Grup(t, 7, [i])]

# ---------------------------------------------------------------- placed in the New Vegas Medical Clinic's operating room, by the Auto-Doc
# Hunter stands in the cleared lecture corner, the new Auto-Doc at his side
HUNTER_POS = (2050.0, -90.0, 10114.0, 0.0, 0.0, 0.0)
HUNTER_AUTODOC = (2020.0, -255.0, 10190.0, 0.0, 0.0, 1.57)   # (the model sits 78 above its centre)
REF_ID = new()
achr = Rec('ACHR', REF_ID, flags=0x400, subs=[['EDID', z('FNVVRMCHunterREF')], ['NAME', fid(NPC_ID)],
                                              ['DATA', f32(*HUNTER_POS)]])
cellov = cell.copy()

# (2.13.2 additions - new form IDs come after the ones above so saves keep Hunter)
# the greeting once you've met him
g_info2 = info("My favorite Courier returns. How can I help you today?", choices=ALL)
g_info2.set('PNAM', fid(g_info.fid))
# first meeting only: GetTalkedToPC == 0
g_info.subs.insert([i for i, x in enumerate(g_info.subs) if x[0] == 'CTDA'][0] + 1,
                   ['CTDA', struct.pack('<B3sfHHIIII', 0, b'\0\0\0', 0.0, 50, 0, 0, 0, 0, 0)])
dial_items[1].items.append(g_info2)

# an Auto-Doc beside him (the clinic's own Auto-Doc model, as a plain static)
_, objs = read_plugin(UP + 'FalloutNV.esm', want={'ACTI', 'CONT'})
FNV_CONT = {r.edid(): r for r in walk(objs) if r.typ == 'CONT'}
ad = [r for r in walk(objs) if r.edid() == 'MedicalClinicAutoDoc'][0]
STAT_ID = new()
adstat = Rec('STAT', STAT_ID, subs=[['EDID', z('FNVVRMCHunterAutoDoc')], ['OBND', ad.get('OBND')], ['MODL', ad.get('MODL')]])
AD_REF = new()
adref = Rec('REFR', AD_REF, subs=[['NAME', fid(STAT_ID)], ['DATA', f32(*HUNTER_AUTODOC)]])

# the lecture corner of the operating room cleared: the medical student, the chalkboards and the chairs (initially disabled)
GONE = ('NVMedicalStudentAM', 'Chalkboard02', 'ChalkboardDirty02', 'BlackboardMarker', 'SchoolChairDirty01', 'NavalChair01F')
objn = pickle.load(open('/home/claude/hunter/data/objnames.pkl', 'rb'))
objn.update({r.fid: (r.typ, r.edid()) for r in walk(small)})
def cell_children(items, cf):
    for x in items:
        if isinstance(x, Grup):
            if x.gtype == 6 and x.label == cf: return x
            g = cell_children(x.items, cf)
            if g: return g
kids = cell_children(cells, 0xED19F)
pers, temp = [achr], [adref]
for g in kids.items:
    for r in walk(g.items):
        if r.typ not in ('REFR', 'ACHR') or not r.get('DATA'): continue
        p = struct.unpack('<6f', r.get('DATA'))
        base = objn.get(struct.unpack('<I', r.get('NAME'))[0], (None, None))[1]
        if base in GONE and 1880 < p[0] < 2176 and -400 < p[1] < 60 and abs(p[2] - 10112) < 150:
            o = r.copy(); o.flags |= 0x800
            (pers if g.gtype == 8 else temp).append(o)
            print('  disabled', base, [round(v) for v in p[:3]])
# Boba: Hunter's cyberdog (Rex's cyberdog body, his own dog: NCR, essential, stays by Hunter)
_, crea = read_plugin(UP + 'FalloutNV.esm', want={'CREA'})
rex = [r for r in walk(crea) if r.edid() == 'Rex'][0]
BOBA_ID = new()
boba = rex.copy(); boba.fid = BOBA_ID
subs = []
for s_, d_ in boba.subs:
    if s_ in ('SCRI', 'PKID', 'SNAM'): continue
    subs.append([s_, d_])
    if s_ == 'ACBS':
        subs.append(['SNAM', fid(FNV['NCRFactionNV'].fid) + bytes(4)])
        subs.append(['SNAM', fid(0x7E26A) + bytes(4)])   # CreatureHitDog
    if s_ == 'AIDT':
        subs.append(['PKID', fid(FNV['DefaultSandboxEditorLocation256NoSleepOrWander'].fid)])
boba.subs = subs
boba.set('EDID', z('FNVVRMCBoba')); boba.set('FULL', z('Boba'))
boba.set('NIFZ', b'eyessetyellow.nif\0boba.nif\0boba_skullcap.nif\0\0')   # black metal body, red glass over the brain
ac = bytearray(boba.get('ACBS')); struct.pack_into('<I', ac, 0, struct.unpack_from('<I', ac, 0)[0] | 0x02); boba.set('ACBS', bytes(ac))
BOBA_REF = new()
bobaref = Rec('ACRE', BOBA_REF, flags=0x400, subs=[['EDID', z('FNVVRMCBobaREF')], ['NAME', fid(BOBA_ID)],
                                                  ['DATA', f32(2120.0, -40.0, 10114.0, 0.0, 0.0, 3.6)]])
pers.append(bobaref)
# a footlocker with a spare set of his gear, lock level 100 (Very Hard)
fl = FNV_CONT['Footlocker01Empty'].copy()
CHEST_ID = new(); fl.fid = CHEST_ID; strip_hashes(fl)
fl.subs = [x for x in fl.subs if x[0] not in ('CNTO', 'COED', 'SCRI')]
fl.set('EDID', z('FNVVRMCHunterFootlocker')); fl.set('FULL', z("Hunter's Footlocker"))
i_ = [k for k, x in enumerate(fl.subs) if x[0] == 'MODL'][0] + 1
for item, n_ in ((ARM_ID, 1), (HLM_ID, 1), (VEC_ID, 1), (AMMO, 200)):
    fl.subs.insert(i_, ['CNTO', fid(item) + struct.pack('<i', n_)]); i_ += 1
CHEST_REF = new()
chestref = Rec('REFR', CHEST_REF, subs=[['NAME', fid(CHEST_ID)], ['XLOC', bytes([100, 0, 0, 0]) + fid(0) + bytes(12)],
                                        ['DATA', f32(2140.0, -300.0, 10112.0, 0.0, 0.0, 4.71)]])
temp.append(chestref)
ENCH_ID = new()
ench = Rec('ENCH', ENCH_ID, subs=[['EDID', z('FNVVRMCHunterGearEnch')], ['FULL', z("Hunter's Chrome-Lined Gear")],
                                  ['ENIT', struct.pack('<IIIB3s', 3, 0, 0, 0, b'\0\0\0')],          # apparel enchantment
                                  ['EFID', fid(0x6AA5D)], ['EFIT', struct.pack('<IIIIi', 20, 0, 0, 0, 14)],   # IncreaseCritChance +20
                                  ['EFID', fid(0xF3D42)], ['EFIT', struct.pack('<IIIIi', 2, 0, 0, 0, 16)]])   # Regen: 2 health a second
light_and_enchanted(arm, ENCH_ID); light_and_enchanted(hlm, ENCH_ID)
cell_grp = Grup(b'CELL', 0, [Grup(7, 2, [Grup(6, 3, [cellov, Grup(0xED19F, 6, [Grup(0xED19F, 8, pers), Grup(0xED19F, 9, temp)])])])])

# ---------------------------------------------------------------- (2.13.4) after every DLC: Joshua Graham, the scriptures, the key
KEY_ID = new()
_, keys = read_plugin(UP + 'FalloutNV.esm', want={'KEYM'})
key = [r for r in walk(keys) if r.edid() == 'VFSVanGraffKey'][0].copy(); key.fid = KEY_ID   # (an ordinary key: model, icons, sounds)
key.set('EDID', z('FNVVRMCHunterFootlockerKey')); key.set('FULL', z("Hunter's Footlocker Key"))
fl.subs.insert([k for k, x in enumerate(fl.subs) if x[0] == 'CNTO'][-1] + 1, ['CNTO', fid(0x0000000F) + struct.pack('<i', 10000)])   # 10,000 caps
chestref.set('XLOC', bytes([100, 0, 0, 0]) + fid(KEY_ID) + bytes(12))
T_GRAHAM, T_SCRIPT = new(), new()
def qdone(q, orflag=False):   # GetQuestCompleted <quest> == 1
    return ['CTDA', struct.pack('<B3sfHHIIII', 0x01 if orflag else 0, b'\0\0\0', 1.0, 546, 0, q, 0, 0, 0)]
ALL_DLCS = [qdone(0x04000FC1),                                   # Dead Money: Heist of the Centuries
            qdone(0x03008A36, True), qdone(0x03008A37),          # Honest Hearts: Crush the White Legs OR Flight from Zion
            qdone(0x05002FCC),                                   # Old World Blues: Old World Blues
            qdone(0x01003615, True), qdone(0x0100BF03)]          # Lonesome Road: The End OR The Apocalypse
gi = info("I wouldn't expect him to remember me. After his centurions went down under the .50s' cover fire, he could only see red. "
          "Caesar would have him killed for his failure to take the dam, so everything else from that day was probably lost to him. "
          "I've heard he reads the scriptures. Maybe God can forgive him. But I can't. "
          "Colossians 3:13. \"Bear with each other and forgive one another if any of you has a grievance against someone. "
          "Forgive as the Lord forgave you.\"", prompt="I've seen Joshua Graham. He didn't mention you.", choices=(T_SCRIPT,) + ALL)
for c in ALL_DLCS: gi.subs.insert([k for k, x in enumerate(gi.subs) if x[0] == 'CTDA'][-1] + 1, c)
si = info("The Bible holds many words of guidance for us, in times of peace and in times of war. "
          "Love your neighbor, and forgive those who have transgressed against you. "
          "All that is to say, I may never forgive the Legion or Caesar for what happened to me. "
          "I will never know the feeling of holding something in my hands again, or of dirt under my bare feet. "
          "They took that away from me on the orders of a coward. "
          "My fight is over, Courier, but yours isn't. Not till the dam is secured and the Legion is no more. "
          "I want you to have this. You've done the Mojave a service. The NCR itself won't show you appreciation, but I can. "
          "May the Holy Spirit guide you to him, and good luck. You're gonna need it.",
          prompt="So you've read the scriptures too, then?", choices=ALL)
si.set('DATA', bytes([0, 0, 0x04, 0]))   # (say once: the key is given one time)
# player.AddItem FNVVRMCHunterFootlockerKey 1  - compiled the way the game's own result scripts are (AddItem = 0x1002)
scda = bytes.fromhex('1c000100') + bytes.fromhex('0210') + struct.pack('<HH', 10, 2) + b'r' + struct.pack('<H', 2) + b'n' + struct.pack('<i', 1)
k0 = [k for k, x in enumerate(si.subs) if x[0] == 'SCHR'][0]
si.subs[k0:k0 + 1] = [['SCHR', struct.pack('<IIIIHH', 0, 2, len(scda), 0, 0, 1)], ['SCDA', scda],
                      ['SCTX', b'player.AddItem FNVVRMCHunterFootlockerKey 1'], ['SCRO', fid(0x14)], ['SCRO', fid(KEY_ID)]]
dial_items += [Rec('DIAL', T_GRAHAM, subs=[['EDID', z('FNVVRMCHunterTopicGraham')], ['QSTI', fid(QST_ID)], ['FULL', z("I've seen Joshua Graham. He didn't mention you.")], ['PNAM', f32(50.0)], ['DATA', b'\0\0']]),
               Grup(T_GRAHAM, 7, [gi]),
               Rec('DIAL', T_SCRIPT, subs=[['EDID', z('FNVVRMCHunterTopicScripture')], ['QSTI', fid(QST_ID)], ['FULL', z("So you've read the scriptures too, then?")], ['PNAM', f32(50.0)], ['DATA', b'\0\0']]),
               Grup(T_SCRIPT, 7, [si])]
# (2.13.5) Caesar dead (GetDeadCount Caesar >= 1)
T_CAESAR = new()
CAESAR = 0x00121FEF   # FortCaesar (the NPC)
ci = info("Good, that'll slow 'em down. Won't bring my friends back, or my arms. Or my legs. "
          "But it can bring some peace to Boone, after all the horrors he had to witness and the loss he had to bear. "
          "Lanius is next in line, the only one they'll listen to the way they listened to Caesar. "
          "With them both gone, the Legion will fall, and maybe the Mojave will finally be rid of their filth. "
          "Next the Fiends and the raiders. Maybe with a robot army those goals wouldn't seem so far away. "
          "Oh well, there isn't one of those anywhere around here. But there is some new chrome, if you want to take a look.",
          prompt="Caesar is dead. I killed him.", choices=ALL)
ci.set('DATA', bytes([0, 0, 0x04, 0]))   # (said once)
ci.subs.insert([k for k, x in enumerate(ci.subs) if x[0] == 'CTDA'][-1] + 1,
               ['CTDA', struct.pack('<B3sfHHIIII', 0x60, b'\0\0\0', 1.0, 84, 0, CAESAR, 0, 0, 0)])
dial_items += [Rec('DIAL', T_CAESAR, subs=[['EDID', z('FNVVRMCHunterTopicCaesar')], ['QSTI', fid(QST_ID)], ['FULL', z("Caesar is dead. I killed him.")], ['PNAM', f32(50.0)], ['DATA', b'\0\0']]),
               Grup(T_CAESAR, 7, [ci])]
# (2.13.6) the NCR, the Legion, and Legion ears for caps
T_NCR, T_LEGION, T_EARS = new(), new(), new()
def topic(t, edid, text):
    return Rec('DIAL', t, subs=[['EDID', z(edid)], ['QSTI', fid(QST_ID)], ['FULL', z(text)], ['PNAM', f32(50.0)], ['DATA', b'\0\0']])
def cond(fn, p1, p2=0, op=0, val=1.0, runon=0, orflag=False):
    return ['CTDA', struct.pack('<B3sfHHIIII', op | (0x01 if orflag else 0), b'\0\0\0', val, fn, 0, p1, p2, runon, 0)]
def add_conds(r, cs):
    for c in cs: r.subs.insert([k for k, x in enumerate(r.subs) if x[0] == 'CTDA'][-1] + 1, c)
ni = info("I joined when I was a kid. Get food and shelter, learn to keep your head on your shoulders. "
          "Then we started expanding our borders. I joined the Rangers, and more and more death followed wherever we went. "
          "We slaughtered the Brotherhood at HELIOS One, and then the Legion got on our radar, and the rest is history. A very painful history. "
          "They have good intentions, and compared to the competition, well, there isn't much to debate.",
          prompt="How do you feel about the NCR?", choices=ALL)
li = info("Took everything from me. Enough said. Killers, rapists, pedophiles... every sin you can think of, they live for. "
          "Give me Legion ears and I'll give you caps.", prompt="How do you feel about the Legion?", choices=ALL)
LEGION_EAR, REP_NCR = 0x00125F4C, 0x000F43DE
GOOD_NCR = [cond(575, REP_NCR, 0, op=0x60, val=2.0),    # GetReputationThreshold RepNVNCR fame >= 2
            cond(575, REP_NCR, 1, op=0x80, val=2.0)]    # ... and infamy < 2  (the NCR likes you)
HAS_EARS = [cond(47, LEGION_EAR, 0, op=0x60, val=1.0, runon=1)]   # the player (the one talked to) has a Legion ear
# the bounty depends on what the NCR thinks of you (FNVR.dll sees "<n> caps an ear" and makes the trade):
# idolized 400, liked 250, anything less (neutral, hated) 100
IDOLIZED = [cond(575, REP_NCR, 0, op=0x60, val=4.0), cond(575, REP_NCR, 1, op=0x80, val=2.0)]
ei = info("Another pile of Legion dogs put down. For a hero of the Republic, 400 caps an ear. Keep 'em coming.",
          prompt="I've got some Legion ears for you.", choices=ALL)
add_conds(ei, HAS_EARS + IDOLIZED)
ei2 = info("Good work. The NCR speaks well of you. 250 caps an ear.", prompt="I've got some Legion ears for you.", choices=ALL)
add_conds(ei2, HAS_EARS + GOOD_NCR)
ei2.set('PNAM', fid(ei.fid))
ei3 = info("The NCR doesn't think much of you, but a dead legionary is a dead legionary. 100 caps an ear.",
           prompt="I've got some Legion ears for you.", choices=ALL)
add_conds(ei3, HAS_EARS)
ei3.set('PNAM', fid(ei2.fid))
dial_items += [topic(T_NCR, 'FNVVRMCHunterTopicNCR', 'How do you feel about the NCR?'), Grup(T_NCR, 7, [ni]),
               topic(T_LEGION, 'FNVVRMCHunterTopicLegion', 'How do you feel about the Legion?'), Grup(T_LEGION, 7, [li]),
               topic(T_EARS, 'FNVVRMCHunterTopicEars', "I've got some Legion ears for you."), Grup(T_EARS, 7, [ei, ei2, ei3])]

# every answer offers the full list of his topics (each only shows when its conditions are met); Goodbye stays last
TOPICS = [T_CHROME, T_OPEN, T_BURNED, T_NCR, T_LEGION, T_EARS, T_GRAHAM, T_CAESAR, T_BYE]
for r in walk(dial_items):
    if r.typ == 'INFO' and any(x[0] == 'TCLT' for x in r.subs):
        first = [k for k, x in enumerate(r.subs) if x[0] == 'TCLT'][0]
        extra = [fid(T_SCRIPT)] if r is gi else []
        r.subs = [x for x in r.subs if x[0] != 'TCLT']
        r.subs[first:first] = [['TCLT', t] for t in extra + [fid(t) for t in TOPICS]]

# ---------------------------------------------------------------- write
top = [
    Grup(b'GLOB', 0, [glob]),
    Grup(b'ENCH', 0, [ench]),
    Grup(b'ARMO', 0, [arm, hlm]),
    Grup(b'WEAP', 0, [vec]),
    Grup(b'STAT', 0, [adstat]),
    Grup(b'KEYM', 0, [key]),
    Grup(b'CONT', 0, [fl]),
    Grup(b'NPC_', 0, [npc]),
    Grup(b'CREA', 0, [boba]),
    cell_grp,
    Grup(b'QUST', 0, [qst]),
    Grup(b'DIAL', 0, dial_items),
]
nrec = sum(1 for _ in walk(top)) + sum(1 for g in top for _ in [0])
def count(items): return sum(1 + (count(x.items) if isinstance(x, Grup) else 0) for x in items)
hdr = Rec('TES4', 0, subs=[['HEDR', struct.pack('<fiI', 1.34, count(top), nid[0])], ['CNAM', z('FNVVRMC')],
                           ['SNAM', z("Hunter - an NCR veteran ripperdoc at the New Vegas Medical Clinic (FNVVRMC cyberware).")]])
for m in ('FalloutNV.esm', 'LonesomeRoad.esm', 'Kriss.esp', 'HonestHearts.esm', 'DeadMoney.esm', 'OldWorldBlues.esm'):
    hdr.subs += [['MAST', z(m)], ['DATA', bytes(8)]]
open(OUT, 'wb').write(hdr.pack() + b''.join(g.pack() for g in top))
print('wrote', OUT, 'records', count(top), 'next id', hex(nid[0]))
for g in g_info, : pass
