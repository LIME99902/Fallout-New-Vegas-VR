/* ==== Cyberware: a ripperdoc, implant slots and implants =============================================================
   Included by fnvr.c (after the settings panel, whose overlay, drawing surface and pointer it shares).

   Hunter at the New Vegas Medical Clinic (FNVVRMC - Hunter.esp) is the ripperdoc: his chrome topic opens the cyberware screen over the
   dialogue (B closes it and the talk goes on). Every implant goes in a body slot; each costs caps and takes some of your
   body's capacity (more with Endurance and level). Most change your stats with the game's own ModAV; what each one
   changed is written down at the moment it's installed, so taking it out undoes exactly that, even if a later version
   changes the numbers. The installed list is kept beside each save (xNVSE's co-save) and cleared for a new game.

   Special implants: the Neural Core holds one active ability - squeeze both grips twice quickly to use it (slow time,
   berserk, or optical camo); the Backup Heart brings you back from near death; Shock Absorbers stop fall damage.
   Temporary boosts that are active when the game is saved are undone again when that save is loaded.

   Chrome perks (worked out from what's installed; shown on the ripperdoc screen):
   Both need Endurance 10 (at any level).
   - FULL BORG: every slot filled - 30% less damage taken (the game's own "damage to the player" multipliers x0.7).
   - CHROME OVERLOAD: with more chrome than your body can handle (over capacity), or as a full borg, every hit you land
     has a 2% chance to set it off, and over capacity every kill has a chance equal to how far over you are (20% over =
     20% per kill): for 20 seconds +50% damage dealt (the "damage by the player" multipliers x1.5),
     +50 damage resistance, +50% movement speed, and every kill heals 20% of your health and adds 3 seconds. The game's multipliers aren't part of a save, so
     they're put back whenever a game is left or loaded. Hits and kills come from xNVSE's own OnHit and OnDeath events. */

#define CY_MAXINST 40
#define CY_MAXMOD 4
enum { CY_CORE, CY_CORTEX, CY_OPTICS, CY_CIRC, CY_NERVE, CY_DERMAL, CY_SKEL, CY_IMMUNE, CY_ARMS, CY_HANDS, CY_LEGS, CY_NSLOT };
static const char *const g_cySlotName[CY_NSLOT] = { "NEURAL CORE", "CORTEX", "OPTICS", "CIRCULATORY", "NERVOUS", "DERMAL",
                                                    "SKELETON", "IMMUNE", "ARMS", "HANDS", "LEGS" };
static const int g_cySlotMax[CY_NSLOT] = { 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };   /* (one Neural Core: the grip squeeze sets off one ability) */
enum { CYS_NONE, CYS_HEART, CYS_SLOWMO, CYS_BERSERK, CYS_CAMO, CYS_NOFALL, CYS_DJUMP };
typedef struct { const char *av; int amt; } CyMod;
typedef struct {
    const char *id; int slot; const char *name; int price, load, special;
    CyMod mod[CY_MAXMOD];
    const char *desc;
} CyImpl;
static const CyImpl g_cyCat[] = {
    { "core_overclock", CY_CORE, "Overclock Core", 6000, 10, CYS_SLOWMO, { { 0 } },
      "Squeeze both grips twice: the world slows to a crawl for 8 seconds while you keep moving. Recharges in 60 seconds." },
    { "core_berserk",   CY_CORE, "Berserker Core", 5000, 9, CYS_BERSERK, { { 0 } },
      "Squeeze both grips twice: for 12 seconds you shrug off hits (+25 damage resistance) and hit far harder up close (+25 melee and unarmed damage). Recharges in 60 seconds." },
    { "core_ghost",     CY_CORE, "Ghost Core", 5500, 8, CYS_CAMO, { { 0 } },
      "Squeeze both grips twice: optical camouflage bends light around you for 12 seconds - near invisible, like a Stealth Boy. Recharges in 90 seconds." },
    { "cortex_recall",  CY_CORTEX, "Recall Lattice", 1500, 3, CYS_NONE, { { "Science", 10 }, { "Medicine", 5 } },
      "A lattice of memory cells wired into your frontal lobe. Never forget a formula again." },
    { "cortex_tactic",  CY_CORTEX, "Tactician Chip", 2500, 4, CYS_NONE, { { "CritChance", 5 } },
      "Reads an enemy's stance and gait and marks the soft spots. More critical hits." },
    { "cortex_tongue",  CY_CORTEX, "Silver Tongue Coprocessor", 2000, 3, CYS_NONE, { { "Speech", 10 }, { "Barter", 10 } },
      "Micro-expression analysis whispered straight into your inner ear. Better deals, better lies." },
    { "cortex_ambidex", CY_CORTEX, "Ambidex Coprocessor", 4000, 5, CYS_NONE, { { "Guns", 5 }, { "MeleeWeapons", 5 } },
      "Splits your motor cortex so each hand runs on its own. Anyone else who carries one dual-wields: a second pistol, revolver, automatic rifle or one-handed blade in the other hand." },
    { "optics_hunter",  CY_OPTICS, "Hunter Lenses", 1800, 3, CYS_NONE, { { "Perception", 1 }, { "Guns", 5 } },
      "Replacement eyes with rangefinding and motion highlighting." },
    { "optics_ballist", CY_OPTICS, "Ballistic Overlay", 3500, 5, CYS_NONE, { { "Guns", 10 }, { "EnergyWeapons", 10 } },
      "Projects bullet drop and lead onto everything you look at." },
    { "optics_threat",  CY_OPTICS, "Threat Scanner", 2200, 3, CYS_NONE, { { "Sneak", 10 }, { "CritChance", 2 } },
      "Tags anyone who's spotted you and where they're looking. Easier to stay hidden, easier to find the weak spot." },
    { "circ_heart",     CY_CIRC, "Synthetic Heart", 2500, 4, CYS_NONE, { { "Health", 50 } },
      "A pump that never tires. A lot more health." },
    { "circ_adrenal",   CY_CIRC, "Adrenal Pump", 2000, 3, CYS_NONE, { { "ActionPoints", 20 } },
      "Doses you with stimulant on demand. More action points." },
    { "circ_backup",    CY_CIRC, "Backup Heart", 7000, 8, CYS_HEART, { { 0 } },
      "When your health drops below 15%, a second heart kicks in and brings you back to full. Once every 5 minutes." },
    { "nerve_reflex",   CY_NERVE, "Reflex Weave", 3000, 4, CYS_NONE, { { "ActionPoints", 15 }, { "CritChance", 2 } },
      "Synthetic nerve fibre that fires faster than flesh." },
    { "nerve_steady",   CY_NERVE, "Steady Signal", 1500, 2, CYS_NONE, { { "Lockpick", 10 }, { "Repair", 10 } },
      "Filters the tremor out of your hands. Fine work comes easy." },
    { "nerve_pain",     CY_NERVE, "Pain Editor", 3500, 5, CYS_NONE, { { "DamageThreshold", 2 }, { "Health", 15 } },
      "Filters pain signals before they reach the brain. Small hits just don't register (+2 damage threshold)." },
    { "derm_plate",     CY_DERMAL, "Subdermal Plating", 3000, 5, CYS_NONE, { { "DamageResist", 10 } },
      "Overlapping ceramic plates under the skin. More damage resistance." },
    { "derm_thresh",    CY_DERMAL, "Ballistic Weave", 4500, 6, CYS_NONE, { { "DamageThreshold", 3 } },
      "Aramid mesh woven through the muscle. Shrugs off small hits completely (+3 damage threshold)." },
    { "derm_fire",      CY_DERMAL, "Thermal Mesh", 1500, 2, CYS_NONE, { { "FireResist", 25 } },
      "Heat-sinking filaments under the skin. Fire hurts a lot less." },
    { "skel_titan",     CY_SKEL, "Titanium Bones", 4000, 6, CYS_NONE, { { "CarryWeight", 50 }, { "Health", 25 } },
      "Every bone laced with titanium. Carry far more, take more punishment." },
    { "skel_spine",     CY_SKEL, "Spinal Brace", 2500, 4, CYS_NONE, { { "CarryWeight", 25 }, { "DamageResist", 5 } },
      "A carbon brace fused along the spine. Carry more, stand up to more." },
    { "skel_lattice",   CY_SKEL, "Bone Lattice", 3000, 4, CYS_NONE, { { "Health", 40 } },
      "A polymer lattice grown through the marrow. Bones that bend instead of breaking." },
    { "imm_filter",     CY_IMMUNE, "Toxin Filter", 1500, 2, CYS_NONE, { { "PoisonResist", 30 }, { "RadResist", 20 } },
      "A synthetic liver and lymph filter. Poison and radiation hurt less." },
    { "imm_rad",        CY_IMMUNE, "Rad Sponge", 2000, 3, CYS_NONE, { { "RadResist", 40 } },
      "A chelating gland that soaks up radiation before it settles in your cells." },
    { "imm_venom",      CY_IMMUNE, "Antivenom Gland", 1800, 2, CYS_NONE, { { "PoisonResist", 40 } },
      "Brews antivenom on the spot. Cazador and radscorpion stings hurt far less." },
    { "arm_kinetic",    CY_ARMS, "Arm Blades: Kinetic", 5000, 7, CYS_NONE, { { "UnarmedDamage", 15 } },
      "Serrated blades that rise out of your forearms. (Their arm model and deploy come in the next update - for now: +15 unarmed damage.)" },
    { "arm_thermal",    CY_ARMS, "Arm Blades: Thermal", 6500, 8, CYS_NONE, { { "UnarmedDamage", 12 }, { "FireResist", 10 } },
      "Blades with heating elements along the edge. (Arm model and fire effect come in the next update - for now: +12 unarmed damage.)" },
    { "arm_shock",      CY_ARMS, "Arm Blades: Shock", 6500, 8, CYS_NONE, { { "UnarmedDamage", 12 } },
      "Blades that arc with current. (Arm model and shock effect come in the next update - for now: +12 unarmed damage.)" },
    { "arm_toxin",      CY_ARMS, "Arm Blades: Toxin", 6500, 8, CYS_NONE, { { "UnarmedDamage", 12 }, { "PoisonResist", 10 } },
      "Blades that weep venom. (Arm model and poison effect come in the next update - for now: +12 unarmed damage.)" },
    { "arm_hydraulic",  CY_ARMS, "Hydraulic Forearms", 3000, 5, CYS_NONE, { { "MeleeDamage", 10 } },
      "Hydraulic rams along both forearms. Every swing of a melee weapon lands harder (+10 melee damage)." },
    { "arm_recoil",     CY_ARMS, "Recoil Dampers", 2500, 4, CYS_NONE, { { "Guns", 8 } },
      "Shock absorbers in the elbow and shoulder joints soak up every kick." },
    { "hand_knuckle",   CY_HANDS, "Ripper Knuckles", 1500, 2, CYS_NONE, { { "UnarmedDamage", 8 } },
      "Tungsten knuckle caps. Punches land harder." },
    { "hand_link",      CY_HANDS, "Smart-Link Palms", 2500, 3, CYS_NONE, { { "Guns", 5 }, { "EnergyWeapons", 5 } },
      "Contacts in your palms talk to your gun. Steadier, more accurate shooting." },
    { "hand_grip",      CY_HANDS, "Grip Servos", 2000, 3, CYS_NONE, { { "MeleeWeapons", 10 }, { "Unarmed", 10 } },
      "Servos in every finger joint. Your grip never slips, your strikes land true." },
    { "leg_sprint",     CY_LEGS, "Sprint Pistons", 2500, 4, CYS_NONE, { { "SpeedMult", 10 } },
      "Hydraulic assists in the calves. You move 10% faster." },
    { "leg_jump",       CY_LEGS, "Kinetic Calves", 4500, 6, CYS_DJUMP, { { 0 } },
      "Stored-energy actuators in your calves. Press jump again in mid-air to jump a second time." },
    { "leg_absorb",     CY_LEGS, "Shock Absorbers", 3000, 4, CYS_NOFALL, { { 0 } },
      "Dampers in your knees and ankles. Falls never hurt you." },
};
enum { CY_NCAT = sizeof g_cyCat / sizeof g_cyCat[0] };

typedef struct { char av[20]; int32_t amt; } CySavedMod;
typedef struct { char id[24]; int32_t n; CySavedMod m[CY_MAXMOD]; } CyInst;
static CyInst g_cyInst[CY_MAXINST]; static int g_cyN = 0;
static CySavedMod g_cyTemp[8]; static int g_cyTempN = 0;          /* an active ability's boosts, undone when it ends */
static CySavedMod g_cyCoTemp[4]; static int g_cyCoTempN = 0;     /* Chrome Overload's boosts */
static CySavedMod g_cyRevert[12]; static int g_cyRevertN = 0;     /* boosts that were active when the loaded save was made */
static volatile LONG g_cyHits = 0, g_cyKills = 0;                  /* (from xNVSE's events: hits and kills by you, not handled yet) */
static volatile bool g_cyJumpEdge = false;
static bool g_cyBorg = false, g_cyCO = false; static double g_cyCOEnd = 0, g_cyCOReady = 0;
static bool g_cyAtDoc = false;                                     /* talking to the ripperdoc right now */
static volatile bool g_cyActivate = false;                         /* the two-squeeze gesture happened */
static NVSESerializationInterface *g_ser = NULL;
static int g_cyCapsShown = 0, g_cyCapShown = 36, g_cyEnd = 0, g_cyPlvShown = 1;
static void Ce_Event(void *attacker, void *victim, bool kill);
#define CY_PERK_END 10   /* Endurance needed for the chrome perks (any level) */                   /* (read on the game's thread, shown by the panel) */
static void Cyber_Open(bool atDoc);

static const CyImpl *Cy_Find(const char *id) { for (int i = 0; i < CY_NCAT; i++) if (!strcmp(g_cyCat[i].id, id)) return &g_cyCat[i]; return NULL; }
static int Cy_InstIdx(const char *id) { for (int i = 0; i < g_cyN; i++) if (!strcmp(g_cyInst[i].id, id)) return i; return -1; }
static int Cy_SlotUsed(int slot) { int n = 0; for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c && c->slot == slot) n++; } return n; }
static int Cy_LoadUsed(void) { int n = 0; for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c) n += c->load; } return n; }
static bool Cy_Has(int special) { for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c && c->special == special) return true; } return false; }
static const CyImpl *Cy_Core(void) { for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c && c->slot == CY_CORE) return c; } return NULL; }

/* ---- the game's own script commands (compiled once each) ---- */
static void *Cy_Compile(const char *src)
{
    if (!g_scriptIntf || !g_scriptIntf->CompileScript) return NULL;
    void *s = g_scriptIntf->CompileScript(src);
    if (!s) Log("Cyberware: the game wouldn't take a script command: %.80s", src);
    return s;
}
static bool Cy_CallNum(void *scr, int nArgs, int a1, double *out)
{
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    if (!scr || !g_scriptIntf || !g_scriptIntf->CallFunction || !GetPlayer()) return false;
    struct { double num; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    bool ok = nArgs ? ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 1, a1)
                    : ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 0);
    if (out) *out = ok && res.type == 1 ? res.num : 0.0;
    return ok;
}
/* one stat changed by amt (player.ModAV <stat> amt); false if the game doesn't know the stat */
static bool Cy_ModAV(const char *av, int amt)
{
    static struct { char av[20]; void *scr; bool bad; } cache[40]; static int n = 0;
    int k = 0; for (; k < n; k++) if (!strcmp(cache[k].av, av)) break;
    if (k == n) {
        if (n >= 40) return false;
        snprintf(cache[n].av, sizeof cache[n].av, "%s", av);
        char src[160]; snprintf(src, sizeof src, "int iV\nBegin Function { iV }\nplayer.ModAV %s iV\nEnd\n", av);
        cache[n].scr = Cy_Compile(src); cache[n].bad = !cache[n].scr; n++;
    }
    if (cache[k].bad) return false;
    return Cy_CallNum(cache[k].scr, 1, amt, NULL);
}
static int Cy_Caps(void)
{
    static void *s = NULL; if (!s) s = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetItemCount Caps001\nSetFunctionValue iV\nEnd\n");
    double v = 0; return Cy_CallNum(s, 0, 0, &v) ? (int)v : 0;
}
static void Cy_CapsChange(int amt)
{
    static void *take = NULL, *give = NULL;
    if (!take) take = Cy_Compile("int iV\nBegin Function { iV }\nplayer.RemoveItem Caps001 iV 1\nEnd\n");
    if (!give) give = Cy_Compile("int iV\nBegin Function { iV }\nplayer.AddItem Caps001 iV 1\nEnd\n");
    if (amt < 0) Cy_CallNum(take, 1, -amt, NULL); else if (amt > 0) Cy_CallNum(give, 1, amt, NULL);
}
static int Cy_Capacity(void)   /* 16 + Endurance x 4 + your level */
{
    static void *s = NULL; if (!s) s = Cy_Compile("int iV\nint iE\nBegin Function { }\nset iE to player.GetAV Endurance\nset iV to player.GetLevel\nset iV to iE * 4 + iV\nSetFunctionValue iV\nEnd\n");
    double v = 0;
    if (s && Cy_CallNum(s, 0, 0, &v) && v > 0) return 16 + (int)v;
    return 36;
}
static int Cy_Endurance(void)   /* (without chems if the game can say so) */
{
    static void *s = NULL; static bool tried = false;
    if (!tried) { tried = true; s = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetPermanentActorValue Endurance\nSetFunctionValue iV\nEnd\n");
        if (!s) s = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetAV Endurance\nSetFunctionValue iV\nEnd\n"); }
    double v = 0; return Cy_CallNum(s, 0, 0, &v) ? (int)v : 0;
}
static int Cy_LoadUsed(void);
static int Cy_OverPct(void)   /* how far over capacity, in % (0 when not over; at most 100) */
{
    int cap = g_cyCapShown > 0 ? g_cyCapShown : 1, used = Cy_LoadUsed();
    if (used <= cap) return 0;
    int p = (used - cap) * 100 / cap; return p > 100 ? 100 : p < 1 ? 1 : p;
}
static int Cy_PlayerLevel(void)
{
    static void *s = NULL; if (!s) s = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetLevel\nSetFunctionValue iV\nEnd\n");
    double v = 0; return Cy_CallNum(s, 0, 0, &v) && v >= 1 ? (int)v : 1;
}
static void Cy_Refresh(void) { if (g_inGameplay && GetPlayer()) { g_cyCapsShown = Cy_Caps(); g_cyCapShown = Cy_Capacity(); g_cyEnd = Cy_Endurance(); g_cyPlvShown = Cy_PlayerLevel(); } }
static double Cy_HealthFrac(void)
{
    static void *s = NULL; if (!s) s = Cy_Compile("float fV\nBegin Function { }\nset fV to player.GetHealthPercentage\nSetFunctionValue fV\nEnd\n");
    double v = -1; if (!Cy_CallNum(s, 0, 0, &v)) return -1;
    return v > 1.5 ? v / 100.0 : v;
}
static void Cy_HealFull(void)
{
    static void *s = NULL; if (!s) s = Cy_Compile("Begin Function { }\nplayer.RestoreAV Health 10000\nEnd\n");
    Cy_CallNum(s, 0, 0, NULL);
}

/* the game's damage multipliers for each difficulty: 0 = damage BY the player, 1 = damage TO the player. Read once, then
   set to the original x mult (never saved with a game - put back on every load and exit) */
static const char *const g_cyDiff[5] = { "VE", "E", "N", "H", "VH" };
static float g_cyGsOrig[2][5]; static bool g_cyGsHave = false; static float g_cyGsMult[2] = { 1.0f, 1.0f };
static bool Cy_GsRead(void)
{
    static bool failed = false;
    if (g_cyGsHave) return true;
    if (failed) return false;
    for (int w = 0; w < 2; w++) for (int k = 0; k < 5; k++) {
        char src[200]; snprintf(src, sizeof src, "float fV\nBegin Function { }\nset fV to GetNumericGameSetting \"fDiffMultHP%sPC%s\"\nSetFunctionValue fV\nEnd\n", w ? "To" : "By", g_cyDiff[k]);
        void *scr = Cy_Compile(src); double v = 0;
        if (!scr || !Cy_CallNum(scr, 0, 0, &v) || !(v > 0.0)) { Log("Cyberware: couldn't read the game's damage setting fDiffMultHP%sPC%s - Full Borg and Chrome Overload damage won't work", w ? "To" : "By", g_cyDiff[k]); failed = true; return false; }
        g_cyGsOrig[w][k] = (float)v;
    }
    g_cyGsHave = true;
    Log("Cyberware: damage settings by you %.2f/%.2f/%.2f/%.2f/%.2f, to you %.2f/%.2f/%.2f/%.2f/%.2f", g_cyGsOrig[0][0], g_cyGsOrig[0][1], g_cyGsOrig[0][2], g_cyGsOrig[0][3], g_cyGsOrig[0][4],
        g_cyGsOrig[1][0], g_cyGsOrig[1][1], g_cyGsOrig[1][2], g_cyGsOrig[1][3], g_cyGsOrig[1][4]);
    return true;
}
static void Cy_GsSet(int w, float mult)
{
    static void *scr[2][5];
    if (fabsf(mult - g_cyGsMult[w]) < 1e-4f) return;
    if (!Cy_GsRead()) return;
    for (int k = 0; k < 5; k++) {
        if (!scr[w][k]) { char src[220]; snprintf(src, sizeof src, "int iV\nfloat fV\nBegin Function { iV }\nset fV to iV / 10000\nSetNumericGameSetting \"fDiffMultHP%sPC%s\" fV\nEnd\n", w ? "To" : "By", g_cyDiff[k]); scr[w][k] = Cy_Compile(src); }
        Cy_CallNum(scr[w][k], 1, (int)lroundf(g_cyGsOrig[w][k] * mult * 10000.0f), NULL);
    }
    g_cyGsMult[w] = mult;
    Log("Cyberware: damage %s you x%.2f", w ? "to" : "by", mult);
}
static void Cy_HealPct(int pct)
{
    static void *s = NULL;
    if (!s) s = Cy_Compile("int iP\nfloat fC\nfloat fP\nfloat fM\nBegin Function { iP }\nset fC to player.GetAV Health\nset fP to player.GetHealthPercentage\nif fP > 1.5\nset fP to fP / 100\nendif\nif fP > 0.01\nset fM to fC / fP * iP / 100\nplayer.RestoreAV Health fM\nendif\nEnd\n");
    Cy_CallNum(s, 1, pct, NULL);
}

/* ---- installing and removing ---- */
static void Cy_Buzz(int us) { for (int h = 0; h < 2; h++) if (g_vrSystem && g_handIdx[h] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, (unsigned short)us); }
static bool Cy_CanInstallHere(void) { return g_cyAtDoc || g_cyAnywhere; }
static const char *Cy_Install(const CyImpl *c)
{
    static char why[160];
    if (!Cy_CanInstallHere()) return "Only a ripperdoc can do that - see Hunter at the New Vegas Medical Clinic";
    if (Cy_InstIdx(c->id) >= 0) return "Already installed";
    if (g_cyN >= CY_MAXINST) return "No room for more implants";
    if (strstr(c->name, "Arm Blades")) for (int i = 0; i < g_cyN; i++) { const CyImpl *o = Cy_Find(g_cyInst[i].id); if (o && strstr(o->name, "Arm Blades")) { snprintf(why, sizeof why, "You already have %s - one set of blades per forearm", o->name); return why; } }
    if (Cy_SlotUsed(c->slot) >= g_cySlotMax[c->slot]) { snprintf(why, sizeof why, "Your %s slot is full - remove one first", g_cySlotName[c->slot]); return why; }
    int cap = Cy_Capacity(), used = Cy_LoadUsed();
    bool over = used + c->load > cap;   /* (allowed - but too much chrome risks a Chrome Overload) */
    int caps = Cy_Caps();
    if (caps < c->price) { snprintf(why, sizeof why, "Not enough caps: %d needed, you have %d", c->price, caps); return why; }
    Cy_CapsChange(-c->price);
    CyInst *in = &g_cyInst[g_cyN]; memset(in, 0, sizeof *in);
    snprintf(in->id, sizeof in->id, "%s", c->id);
    for (int k = 0; k < CY_MAXMOD && c->mod[k].av; k++) {
        if (Cy_ModAV(c->mod[k].av, c->mod[k].amt)) { snprintf(in->m[in->n].av, sizeof in->m[0].av, "%s", c->mod[k].av); in->m[in->n].amt = c->mod[k].amt; in->n++; }
        else Log("Cyberware: %s - couldn't change %s (left out)", c->name, c->mod[k].av);
    }
    g_cyN++;
    Cy_Buzz(3500);
    Log("Cyberware: installed %s for %d caps (%d stat changes)", c->name, c->price, in->n);
    if (over) snprintf(why, sizeof why, "%s installed - OVER CAPACITY (%d / %d): your hits can now set off a Chrome Overload", c->name, used + c->load, cap);
    else snprintf(why, sizeof why, "%s installed", c->name);
    return why;
}
static const char *Cy_Remove(const CyImpl *c)
{
    static char why[160];
    if (!Cy_CanInstallHere()) return "Only a ripperdoc can take implants out";
    int i = Cy_InstIdx(c->id); if (i < 0) return "Not installed";
    if (c->slot == CY_CORE && g_cyTempN) return "Wait for the ability to finish first";
    for (int k = 0; k < g_cyInst[i].n; k++) Cy_ModAV(g_cyInst[i].m[k].av, -g_cyInst[i].m[k].amt);
    memmove(&g_cyInst[i], &g_cyInst[i + 1], sizeof(CyInst) * (size_t)(g_cyN - i - 1)); g_cyN--;
    int refund = c->price / 4; Cy_CapsChange(refund);
    Cy_Buzz(2000);
    Log("Cyberware: removed %s (refund %d caps)", c->name, refund);
    snprintf(why, sizeof why, "%s removed - %d caps back", c->name, refund);
    return why;
}

/* ---- abilities ---- */
static double g_cyAbilEnd = 0, g_cyAbilReady = 0, g_cyHeartReady = 0; static int g_cyAbil = 0;
static bool g_cySlowSet = false;
static void Cy_TempMod(const char *av, int amt)
{
    if (g_cyTempN < 8 && Cy_ModAV(av, amt)) { snprintf(g_cyTemp[g_cyTempN].av, sizeof g_cyTemp[0].av, "%s", av); g_cyTemp[g_cyTempN].amt = amt; g_cyTempN++; }
}
static void Cy_TempEnd(void)
{
    for (int i = 0; i < g_cyTempN; i++) Cy_ModAV(g_cyTemp[i].av, -g_cyTemp[i].amt);
    g_cyTempN = 0;
}
static void Cy_CoStart(void)
{
    if (g_cyCO) return;
    g_cyCO = true; g_cyCOEnd = QpcMs() + 20000.0;
    static const CyMod boost[2] = { { "DamageResist", 50 }, { "SpeedMult", 50 } };   /* (+50 resistance, +50% movement speed) */
    for (int k = 0; k < 2; k++)
        if (g_cyCoTempN < 4 && Cy_ModAV(boost[k].av, boost[k].amt)) { snprintf(g_cyCoTemp[g_cyCoTempN].av, sizeof g_cyCoTemp[0].av, "%s", boost[k].av); g_cyCoTemp[g_cyCoTempN].amt = boost[k].amt; g_cyCoTempN++; }
    Cy_GsSet(0, 1.5f);
    UIMessage("CHROME OVERLOAD"); Cy_Buzz(3999);
    Log("Cyberware: Chrome Overload! (+50%% damage, +50 damage resistance, +50%% speed, kills heal 20%% and add 3 s, 20 s)");
}
static void Cy_CoEnd(bool undo)
{
    if (!g_cyCO) return;
    if (undo) for (int i = 0; i < g_cyCoTempN; i++) Cy_ModAV(g_cyCoTemp[i].av, -g_cyCoTemp[i].amt);
    g_cyCoTempN = 0; g_cyCO = false; g_cyCOReady = QpcMs() + 30000.0;
    Cy_GsSet(0, 1.0f);
    if (undo) { UIMessage("Chrome Overload wore off"); Log("Cyberware: Chrome Overload over"); }
}
/* xNVSE's events: a hit you land, a kill you make (counted here, handled on the next tick) */
static void Cy_OnHit(void *thisObj, void *params)
{
    (void)thisObj; void **p = (void **)params; void *pl = GetPlayer();
    if (p && pl && p[1] == pl && p[0] && p[0] != pl) InterlockedIncrement(&g_cyHits);
    else if (p && pl && p[1] && p[0] && p[1] != pl) Ce_Event(p[1], p[0], false);   /* (someone else's hit: a chromed enemy's?) */
}
static void Cy_OnDeath(void *thisObj, void *params)
{
    (void)thisObj; void **p = (void **)params; void *pl = GetPlayer();
    if (p && pl && p[1] == pl && p[0] && p[0] != pl) InterlockedIncrement(&g_cyKills);
    else if (p && pl && p[1] && p[0] && p[1] != pl) Ce_Event(p[1], p[0], true);
}
static NVSEInterface *g_cyNvse = NULL;
static void Cyber_Events(void)
{
    static bool done = false; if (done || !g_cyNvse) return; done = true;
    NVSEInterface *nvse = g_cyNvse;
    NVSEEventManagerInterface *em = (NVSEEventManagerInterface *)nvse->QueryInterface(kInterface_EventManager);
    bool h = em && em->SetNativeEventHandler && em->SetNativeEventHandler("onhit", Cy_OnHit);
    bool d = em && em->SetNativeEventHandler && em->SetNativeEventHandler("ondeath", Cy_OnDeath);
    Log("Cyberware: hit events %s, kill events %s (Chrome Overload needs them)", h ? "on" : "NOT AVAILABLE", d ? "on" : "NOT AVAILABLE");
}
static void Cy_SlowTime(bool on)
{
    volatile float *target = (volatile float *)0x011AC3A4;   /* (the game eases its global time multiplier toward this - as VATS does) */
    if (IsBadWritePtr((void *)target, 4)) return;
    if (on) { *target = 0.35f; g_cySlowSet = true; }
    else if (g_cySlowSet) { *target = 1.0f; g_cySlowSet = false; }
}
static void Cy_AbilityStop(const char *why)
{
    if (!g_cyAbil) return;
    if (g_cyAbil == CYS_SLOWMO) Cy_SlowTime(false);
    Cy_TempEnd();
    Log("Cyberware: ability over (%s)", why);
    g_cyAbil = 0;
}
static void Cy_AbilityStart(void)
{
    const CyImpl *c = Cy_Core(); double now = QpcMs();
    if (!c) return;
    if (g_cyAbil) { Cy_Buzz(400); return; }
    if (now < g_cyAbilReady) { char m[96]; snprintf(m, sizeof m, "%s recharging - %d s", c->name, (int)ceil((g_cyAbilReady - now) / 1000.0)); UIMessage(m); Cy_Buzz(400); return; }
    g_cyAbil = c->special;
    double dur = 8000, cool = 60000;
    switch (c->special) {
    case CYS_SLOWMO: Cy_SlowTime(true); dur = 8000; cool = 60000; break;
    case CYS_BERSERK: Cy_TempMod("DamageResist", 25); Cy_TempMod("MeleeDamage", 25); Cy_TempMod("UnarmedDamage", 25); dur = 12000; cool = 60000; break;
    case CYS_CAMO: Cy_TempMod("Chameleon", 1); dur = 12000; cool = 90000; break;
    default: g_cyAbil = 0; return;
    }
    g_cyAbilEnd = now + dur; g_cyAbilReady = now + dur + cool;
    Cy_Buzz(3999);
    char m[96]; snprintf(m, sizeof m, "%s engaged", c->name); UIMessage(m);
    Log("Cyberware: %s engaged", c->name);
}
/* the two-squeeze gesture: both grips squeezed hard, let go, squeezed again - within 0.6 s */
static void Cy_Gesture(const VRControllerState_t *l, const VRControllerState_t *r, bool okL, bool okR)
{
    static int phase = 0; static double at = 0;
    if (!g_cyOn || !okL || !okR || !g_inGameplay || IsMenuMode() || g_pipOpen) { phase = 0; return; }
    {   /* jump (A) pressed: a second jump in the air with Kinetic Calves */
        static bool aPrev = false; bool a = Pressed(r, EVRButtonId_k_EButton_A);
        if (a && !aPrev) g_cyJumpEdge = true;
        aPrev = a;
    }
    float gl = l->rAxis[2].x, gr = r->rAxis[2].x; double now = QpcMs();
    bool both = gl > 0.85f && gr > 0.85f, none = gl < 0.35f && gr < 0.35f;
    if (phase && now - at > 600.0) phase = 0;
    if (phase == 0 && both) { phase = 1; at = now; }
    else if (phase == 1 && none) phase = 2;
    else if (phase == 2 && both) { phase = 0; if (Cy_Core()) g_cyActivate = true; }
}
#include "fnvr_cyber_enemy.c"
static void Cy_Tick(void)
{
    if (g_cyRevertN && g_inGameplay && GetPlayer()) {   /* (a save made while a boost was on: the boost is undone) */
        for (int i = 0; i < g_cyRevertN; i++) Cy_ModAV(g_cyRevert[i].av, -g_cyRevert[i].amt);
        Log("Cyberware: undid %d boosts that were on when that game was saved", g_cyRevertN);
        g_cyRevertN = 0;
    }
    if (!g_cyOn) return;
    {   /* who you're talking to (logged) */
        bool dlg = ((const volatile bool *)0x011F308F)[1009];
        static bool dlgWas = false; static int tries = 0;
        if (dlg && !dlgWas) tries = 20;   /* (who you're talking to may only be set a frame or two after the menu opens) */
        if (dlg && tries > 0) {
            tries--;
            char *tm = *(char **)0x011DDDE4, *who = tm && !IsBadReadPtr(tm + 0x0C, 4) ? *(char **)(tm + 0x0C) : NULL;
            char *base = who && !IsBadReadPtr(who + 0x20, 4) ? *(char **)(who + 0x20) : NULL;
            const char *nm = NULL;
            for (int off = 0xC8; base && off <= 0xE0 && !nm; off += 4) {
                const char *s = !IsBadReadPtr(base + off, 4) ? *(const char **)(base + off) : NULL;
                if (s && !IsBadStringPtrA(s, 64) && s[0] >= 'A' && s[0] <= 'Z' && strlen(s) < 48) nm = s;
            }
            if (nm || !tries) { tries = 0; static int logged = 0; if (logged++ < 40) Log("Cyberware: talking to %s", nm ? nm : "(no name found)"); }
        }
        /* Hunter (FNVVRMC - Hunter.esp, New Vegas Medical Clinic), the ripperdoc: his "What kind of chrome do you have?" topic sets the
           global FNVVRMCHunterChrome to 1; it's read (and cleared) here while you're talking */
        static void *hScr = NULL; static bool hBad = false; static int hWait = 0;
        if (dlg && !hBad && ++hWait >= 10) {
            hWait = 0;
            if (!hScr) {
                hScr = g_scriptIntf && g_scriptIntf->CompileScript ? g_scriptIntf->CompileScript(
                    "int iV\nBegin Function { }\nset iV to FNVVRMCHunterChrome\nset FNVVRMCHunterChrome to 0\nSetFunctionValue iV\nEnd\n") : NULL;
                if (!hScr) { hBad = true; Log("Cyberware: Hunter's plugin isn't loaded (no FNVVRMCHunterChrome) - no ripperdoc; install FNVVRMC - Hunter.esp"); }
            }
            double v = 0;
            if (hScr && Cy_CallNum(hScr, 0, 0, &v) && v > 0.5) {
                g_cyAtDoc = true;
                Log("Cyberware: Hunter - opening the ripperdoc");
                Cyber_Open(true);
            }
        }
        if (!dlg && dlgWas) g_cyAtDoc = false;
        dlgWas = dlg;
    }
    if (!g_inGameplay || !GetPlayer()) return;
    double now = QpcMs();
    g_cyPlayerLvl = g_cyPlvShown;
    Ce_Tick(now);
    if (g_cyActivate) { g_cyActivate = false; Cy_AbilityStart(); }
    if (g_cyAbil && now > g_cyAbilEnd) { Cy_AbilityStop("time up"); Cy_Buzz(1500); }
    if (g_cyAbil == CYS_SLOWMO && ((const volatile bool *)0x011F308F)[1056]) Cy_AbilityStop("VATS");   /* (VATS runs time itself) */
    {   /* chrome perks */
        static double nextLook = 0;
        if (now > nextLook) {
            nextLook = now + 1500.0;
            Cy_Refresh();
            bool borg = g_cyEnd >= CY_PERK_END; for (int sl = 0; sl < CY_NSLOT; sl++) if (Cy_SlotUsed(sl) < g_cySlotMax[sl]) borg = false;
            if (borg != g_cyBorg) {
                g_cyBorg = borg; UIMessage(borg ? "FULL BORG: 30% less damage taken" : "No longer a full borg");
                Log("Cyberware: Full Borg %s", borg ? "ON" : "off");
            }
            Cy_GsSet(1, g_cyBorg ? 0.7f : 1.0f);
        }
        LONG hits = InterlockedExchange(&g_cyHits, 0), kills = InterlockedExchange(&g_cyKills, 0);
        bool risk = g_cyEnd >= CY_PERK_END && (g_cyBorg || Cy_LoadUsed() > g_cyCapShown);
        if (hits && risk && !g_cyCO && now > g_cyCOReady)
            for (LONG i = 0; i < hits && !g_cyCO; i++) if (rand() % 100 < 2) Cy_CoStart();
        {   /* a kill: the chance is how far over capacity you are (20% over = 20% a kill) */
            int pct = Cy_OverPct();
            if (kills && risk && pct > 0 && !g_cyCO && now > g_cyCOReady) {
                for (LONG i = 0; i < kills && !g_cyCO; i++) if (rand() % 100 < pct) { Log("Cyberware: a kill set off Chrome Overload (%d%% over capacity)", pct); Cy_CoStart(); kills = 0; }
            }
        }
        if (kills && g_cyCO) {   /* each kill: 20% health back and 3 more seconds */
            Cy_HealPct(20 * (int)(kills > 5 ? 5 : kills));
            g_cyCOEnd += 3000.0 * (double)kills;
            char m[64]; snprintf(m, sizeof m, "Chrome Overload +%ld s", 3L * (long)kills); UIMessage(m);
        }
        if (g_cyCO && now > g_cyCOEnd) Cy_CoEnd(true);
    }
    if (Cy_Has(CYS_DJUMP)) {   /* a second jump in mid-air */
        static bool used = false;
        char *player = (char *)GetPlayer(), *proc = *(char **)(player + 0x68);
        char *ctrl = (proc && *(uint8_t *)(proc + 0x28) <= 1) ? *(char **)(proc + 0x138) : NULL;
        char *proxy = ctrl && !IsBadReadPtr(ctrl + 0x08, 4) ? *(char **)(ctrl + 0x08) : NULL;
        if (ctrl && proxy && !IsBadWritePtr(ctrl + 0x4F0, 0x60)) {
            float fall = *(float *)(ctrl + 0x548);
            if (fall < 0.02f) used = false;
            if (g_cyJumpEdge && fall > 0.12f && !used && !g_climbing && !Grapple_Busy()) {
                used = true;
                float vz = 5.5f * g_unitsPerMeter / HAVOK_UNIT, *pv = (float *)(proxy + 0x10), *cv = (float *)(ctrl + 0x4F0);
                pv[2] = vz; cv[2] = vz;
                *(float *)(ctrl + 0x548) = 0.0f; *(float *)(ctrl + 0x544) = ((float *)(player + 0x30))[2];
                Cy_Buzz(2500);
                static int ln = 0; if (ln++ < 5) Log("Cyberware: double jump");
            }
        }
    }
    g_cyJumpEdge = false;
    static double nextHeart = 0;
    if (now > nextHeart && Cy_Has(CYS_HEART)) {
        nextHeart = now + 200.0;
        double f = Cy_HealthFrac();
        if (f > 0.0 && f < 0.15 && now > g_cyHeartReady) {
            Cy_HealFull(); g_cyHeartReady = now + 300000.0;
            UIMessage("Backup Heart: restarted"); Cy_Buzz(3999);
            Log("Cyberware: Backup Heart brought you back from %.0f%% health", f * 100.0);
        }
    }
    if (Cy_Has(CYS_NOFALL)) {   /* (the fall's starting height kept at where you are: a fall never builds up damage) */
        char *player = (char *)GetPlayer(), *proc = *(char **)(player + 0x68);
        char *ctrl = (proc && *(uint8_t *)(proc + 0x28) <= 1) ? *(char **)(proc + 0x138) : NULL;
        if (ctrl && !IsBadWritePtr(ctrl + 0x544, 4)) *(float *)(ctrl + 0x544) = ((float *)(player + 0x30))[2];
    }
}

/* ---- the co-save ---- */
#define CY_REC 0x43594252u   /* 'CYBR' */
static void Cy_Reset(const char *why)
{   /* (the old game's values go with it - nothing is undone by hand, no game scripts run mid-load) */
    if (g_cyAbil) Log("Cyberware: ability over (%s)", why);
    if (g_cyAbil == CYS_SLOWMO) Cy_SlowTime(false);
    g_cyAbil = 0; g_cyTempN = 0; g_cyAtDoc = false;
    g_cyCO = false; g_cyCoTempN = 0; g_cyCOReady = 0; g_cyBorg = false;
    if (g_cyGsHave) { Cy_GsSet(0, 1.0f); Cy_GsSet(1, 1.0f); }
    g_cyN = 0; g_cyRevertN = 0; g_cyAbilReady = g_cyHeartReady = 0;
    Ce_Reset();
}
static void Cy_SaveCb(void *r)
{
    (void)r;
    if (!g_ser || !g_ser->OpenRecord(CY_REC, 1)) return;
    int32_t n = g_cyN, t = g_cyTempN + g_cyCoTempN;
    g_ser->WriteRecordData(&n, 4);
    if (n) g_ser->WriteRecordData(g_cyInst, (uint32_t)(sizeof(CyInst) * (size_t)n));
    g_ser->WriteRecordData(&t, 4);
    if (g_cyTempN) g_ser->WriteRecordData(g_cyTemp, (uint32_t)(sizeof(CySavedMod) * (size_t)g_cyTempN));
    if (g_cyCoTempN) g_ser->WriteRecordData(g_cyCoTemp, (uint32_t)(sizeof(CySavedMod) * (size_t)g_cyCoTempN));
    Log("Cyberware: saved %d implants with the game%s", n, t ? " (a boost was on)" : "");
    Ce_Save();
}
static void Cy_LoadCb(void *r)
{
    (void)r;
    Cy_Reset("loading a game");
    uint32_t type, ver, len;
    while (g_ser && g_ser->GetNextRecordInfo(&type, &ver, &len)) {
        if (type == CE_REC) { Ce_Load(ver); continue; }
        if (type != CY_REC || ver != 1) continue;
        int32_t n = 0, t = 0;
        if (g_ser->ReadRecordData(&n, 4) != 4 || n < 0 || n > CY_MAXINST) break;
        if (n && g_ser->ReadRecordData(g_cyInst, (uint32_t)(sizeof(CyInst) * (size_t)n)) != sizeof(CyInst) * (size_t)n) { n = 0; break; }
        g_cyN = n;
        for (int i = 0; i < g_cyN; i++) { g_cyInst[i].id[23] = 0; if (g_cyInst[i].n < 0 || g_cyInst[i].n > CY_MAXMOD) g_cyInst[i].n = 0; for (int k = 0; k < CY_MAXMOD; k++) g_cyInst[i].m[k].av[19] = 0; }
        if (g_ser->ReadRecordData(&t, 4) == 4 && t > 0 && t <= 12 && g_ser->ReadRecordData(g_cyRevert, (uint32_t)(sizeof(CySavedMod) * (size_t)t)) == sizeof(CySavedMod) * (size_t)t) {
            for (int i = 0; i < t; i++) g_cyRevert[i].av[19] = 0;
            g_cyRevertN = t;
        }
    }
    Log("Cyberware: loaded %d implants with the game", g_cyN);
}
static void Cy_NewGameCb(void *r) { (void)r; Cy_Reset("new game"); Log("Cyberware: new game - no implants"); }
static void Cyber_Init(NVSEInterface *nvse, PluginHandle h)
{
    g_cyNvse = nvse;
    srand((unsigned)GetTickCount() ^ (unsigned)GetCurrentProcessId());   /* (Chrome Overload's chance) */
    g_cyOn = GetPrivateProfileIntA("Cyberware", "bEnabled", 1, g_iniPath) != 0;
    g_cyAnywhere = GetPrivateProfileIntA("Cyberware", "bRipperdocAnywhere", 0, g_iniPath) != 0;
    g_cyEnemies = GetPrivateProfileIntA("Cyberware", "bChromedEnemies", 1, g_iniPath) != 0;
    g_ceDualAxis = GetPrivateProfileIntA("Cyberware", "iDualMirrorAxis", 2, g_iniPath); if (g_ceDualAxis < 0 || g_ceDualAxis > 2) g_ceDualAxis = 2;
    g_ser = (NVSESerializationInterface *)nvse->QueryInterface(kInterface_Serialization);
    if (g_ser && g_ser->SetSaveCallback && g_ser->SetLoadCallback) {
        g_ser->SetSaveCallback(h, Cy_SaveCb); g_ser->SetLoadCallback(h, Cy_LoadCb);
        if (g_ser->SetNewGameCallback) g_ser->SetNewGameCallback(h, Cy_NewGameCb);
        Log("Cyberware: implants are kept with each save (xNVSE co-save v%u)", g_ser->version);
    } else Log("Cyberware: xNVSE's co-save isn't there - implants won't be kept with saves");
}

/* ---- the ripperdoc screen (on the FNVR settings panel) ---- */
#define CY_TEAL RGB(0, 226, 205)
#define CY_TEALD RGB(0, 110, 100)
#define CY_RED RGB(255, 58, 92)
#define CY_BG RGB(6, 10, 14)
#define CY_PANEL RGB(12, 22, 28)
#define CY_TXT RGB(206, 236, 232)
#define CY_DIM RGB(92, 130, 128)
static int g_cySlot = 0, g_cyItem = 0, g_cyCol = 0, g_cyBtn = 0;   /* column 0 slots, 1 implants, 2 buttons */
static int g_cyHoverKind = 0, g_cyHoverIdx = -1;
static char g_cyMsg[200]; static double g_cyMsgUntil = 0;
static bool g_cyFromDoc = false;
static int Cy_SlotItems(int slot, const CyImpl **out, int max)
{
    int n = 0; for (int i = 0; i < CY_NCAT && n < max; i++) if (g_cyCat[i].slot == slot) out[n++] = &g_cyCat[i]; return n;
}
static void Cyber_Open(bool atDoc)
{
    if (!g_setOpen) Settings_Open();
    if (!g_setOpen) return;
    g_cyFromDoc = atDoc;
    g_setPage = SP_CYBER; g_setDirty = true; g_cyCol = 0; g_cyMsgUntil = 0;
    Cy_Refresh();
}
#define CY_LX0 24
#define CY_LX1 330
#define CY_RX0 350
#define CY_RX1 1000
#define CY_Y0 196
#define CY_SLH 64
#define CY_ITH 64
#define CY_DY0 590
#define CY_BTNY0 1090
#define CY_BTNY1 1160
static void Cy_Btn(int x0, int y0, int x1, int y1, const char *s, bool sel, bool enabled, COLORREF c)
{
    if (sel) Set_Fill(x0, y0, x1, y1, enabled ? c : CY_DIM);
    else { Set_Fill(x0, y0, x1, y0 + 3, enabled ? c : CY_DIM); Set_Fill(x0, y1 - 3, x1, y1, enabled ? c : CY_DIM); Set_Fill(x0, y0, x0 + 3, y1, enabled ? c : CY_DIM); Set_Fill(x1 - 3, y0, x1, y1, enabled ? c : CY_DIM); }
    Set_Text(g_setFontSmall, sel ? CY_BG : enabled ? c : CY_DIM, x0, y0, x1, y1, s, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}
static void Cy_Draw(void)
{
    char t[200];
    const bool doc = Cy_CanInstallHere();
    Set_Fill(0, 0, SET_W, SET_H, CY_BG);
    for (int y = 0; y < SET_H; y += 6) Set_Fill(0, y, SET_W, y + 1, RGB(9, 15, 20));   /* scan lines */
    Set_Fill(0, 0, SET_W, 4, CY_TEAL); Set_Fill(0, SET_H - 4, SET_W, SET_H, CY_TEAL);
    Set_Fill(0, 0, 4, SET_H, CY_TEAL); Set_Fill(SET_W - 4, 0, SET_W, SET_H, CY_TEAL);
    Set_Fill(24, 26, 34, 104, CY_RED);
    Set_Text(g_setFontBig, CY_TEAL, 48, 18, SET_W - 40, 82, doc ? "RIPPERDOC" : "CYBERWARE", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    Set_Text(g_setFontSmall, CY_DIM, 50, 76, SET_W - 40, 112, g_cyAtDoc ? "DOC USANAGI  //  NEW VEGAS MEDICAL CLINIC" : g_cyAnywhere ? "RIPPERDOC ANYWHERE (TESTING) IS ON" : "VIEW ONLY  //  INSTALL AT DOC USANAGI, NEW VEGAS MEDICAL CLINIC", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int caps = g_cyCapsShown; snprintf(t, sizeof t, "%d CAPS", caps);
    Set_Text(g_setFont, CY_TXT, 40, 22, SET_W - 40, 80, t, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    /* capacity */
    int cap = g_cyCapShown, used = Cy_LoadUsed();
    snprintf(t, sizeof t, used > cap ? "OVER CAPACITY  %d / %d" : "BODY CAPACITY  %d / %d", used, cap);
    Set_Text(g_setFontSmall, used > cap ? CY_RED : CY_TXT, 40, 120, 520, 156, t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    int bx0 = 470, bx1 = SET_W - 40, by0 = 128, by1 = 148, fill = cap > 0 ? bx0 + (bx1 - bx0) * (used > cap ? cap : used) / cap : bx0;
    Set_Fill(bx0, by0, bx1, by1, RGB(20, 40, 44)); Set_Fill(bx0, by0, fill, by1, used * 10 > cap * 9 ? CY_RED : CY_TEAL);

    Set_Fill(24, 172, SET_W - 24, 175, CY_TEALD);
    /* slots */
    for (int s = 0; s < CY_NSLOT; s++) {
        int y0 = CY_Y0 + s * CY_SLH, y1 = y0 + CY_SLH - 8;
        bool sel = s == g_cySlot, foc = sel && g_cyCol == 0, hov = g_cyHoverKind == 1 && g_cyHoverIdx == s;
        if (sel) Set_Fill(CY_LX0, y0, CY_LX1, y1, foc ? CY_TEAL : RGB(18, 50, 54));
        else if (hov) Set_Fill(CY_LX0, y0, CY_LX1, y1, RGB(14, 34, 38));
        Set_Fill(CY_LX0, y0, CY_LX0 + 5, y1, Cy_SlotUsed(s) ? CY_RED : CY_TEALD);
        Set_Text(g_setFontSmall, foc ? CY_BG : CY_TXT, CY_LX0 + 18, y0, CY_LX1 - 70, y1, g_cySlotName[s], DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        snprintf(t, sizeof t, "%d/%d", Cy_SlotUsed(s), g_cySlotMax[s]);
        Set_Text(g_setFontSmall, foc ? CY_BG : CY_DIM, CY_LX1 - 80, y0, CY_LX1 - 10, y1, t, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
    /* implants for the slot */
    const CyImpl *it[8]; int ni = Cy_SlotItems(g_cySlot, it, 8);
    if (g_cyItem >= ni) g_cyItem = ni - 1; if (g_cyItem < 0) g_cyItem = 0;
    for (int i = 0; i < ni; i++) {
        int y0 = CY_Y0 + i * CY_ITH, y1 = y0 + CY_ITH - 8;
        bool sel = i == g_cyItem, foc = sel && g_cyCol == 1, hov = g_cyHoverKind == 2 && g_cyHoverIdx == i, inst = Cy_InstIdx(it[i]->id) >= 0;
        Set_Fill(CY_RX0, y0, CY_RX1, y1, foc ? CY_TEAL : sel ? RGB(18, 50, 54) : hov ? RGB(14, 34, 38) : CY_PANEL);
        Set_Text(g_setFont, foc ? CY_BG : CY_TXT, CY_RX0 + 20, y0, CY_RX1 - 170, y1, it[i]->name, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (inst) snprintf(t, sizeof t, "INSTALLED"); else snprintf(t, sizeof t, "%d C", it[i]->price);
        Set_Text(g_setFontSmall, foc ? CY_BG : inst ? CY_RED : (caps >= it[i]->price ? CY_TEAL : CY_DIM), CY_RX1 - 170, y0, CY_RX1 - 20, y1, t, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
    /* details */
    if (ni) {
        const CyImpl *c = it[g_cyItem]; int ii = Cy_InstIdx(c->id); bool inst = ii >= 0;
        Set_Fill(CY_RX0, CY_DY0, CY_RX1, CY_BTNY1 + 20, CY_PANEL);
        Set_Fill(CY_RX0, CY_DY0, CY_RX1, CY_DY0 + 4, inst ? CY_RED : CY_TEAL);
        Set_Text(g_setFont, CY_TEAL, CY_RX0 + 22, CY_DY0 + 16, CY_RX1 - 20, CY_DY0 + 66, c->name, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        snprintf(t, sizeof t, "%s  //  CAPACITY %d  //  %d CAPS", g_cySlotName[c->slot], c->load, c->price);
        Set_Text(g_setFontSmall, CY_DIM, CY_RX0 + 22, CY_DY0 + 66, CY_RX1 - 20, CY_DY0 + 100, t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Text(g_setFontSmall, CY_TXT, CY_RX0 + 22, CY_DY0 + 112, CY_RX1 - 22, CY_DY0 + 300, c->desc, DT_LEFT | DT_WORDBREAK);
        int ey = CY_DY0 + 310;
        for (int k = 0; k < CY_MAXMOD && c->mod[k].av; k++, ey += 40) {
            snprintf(t, sizeof t, "%+d  %s", c->mod[k].amt, c->mod[k].av);
            Set_Fill(CY_RX0 + 22, ey + 14, CY_RX0 + 34, ey + 26, CY_TEAL);
            Set_Text(g_setFontSmall, CY_TXT, CY_RX0 + 48, ey, CY_RX1 - 20, ey + 40, t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        if (c->special) {
            const char *sp = c->special == CYS_HEART ? "AUTOMATIC: saves you below 15% health" : c->special == CYS_NOFALL ? "ALWAYS ON: no fall damage" :
                             c->special == CYS_DJUMP ? "ALWAYS ON: jump again in mid-air" : "ACTIVE ABILITY: squeeze both grips twice";
            Set_Fill(CY_RX0 + 22, ey + 14, CY_RX0 + 34, ey + 26, CY_RED);
            Set_Text(g_setFontSmall, CY_RED, CY_RX0 + 48, ey, CY_RX1 - 20, ey + 40, sp, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        int mid = (CY_RX0 + CY_RX1) / 2;
        snprintf(t, sizeof t, "INSTALL  %d C", c->price);
        Cy_Btn(CY_RX0 + 22, CY_BTNY0, mid - 10, CY_BTNY1, t, (g_cyCol == 2 && g_cyBtn == 0) || (g_cyHoverKind == 3 && g_cyHoverIdx == 0), doc && !inst, CY_TEAL);
        snprintf(t, sizeof t, "REMOVE  +%d C", c->price / 4);
        Cy_Btn(mid + 10, CY_BTNY0, CY_RX1 - 22, CY_BTNY1, t, (g_cyCol == 2 && g_cyBtn == 1) || (g_cyHoverKind == 3 && g_cyHoverIdx == 1), doc && inst, CY_RED);
    }
    /* message line + help */
    if (QpcMs() < g_cyMsgUntil) Set_Text(g_setFont, CY_RED, 30, 900, CY_LX1 + 4, 1180, g_cyMsg, DT_LEFT | DT_WORDBREAK);
    else {   /* chrome perks */
        int filled = 0, total = 0; for (int sl = 0; sl < CY_NSLOT; sl++) { int u = Cy_SlotUsed(sl); filled += u < g_cySlotMax[sl] ? u : g_cySlotMax[sl]; total += g_cySlotMax[sl]; }
        bool over = used > cap, endOk = g_cyEnd >= CY_PERK_END;
        char coTxt[96]; snprintf(coTxt, sizeof coTxt, "%d%% over: %d%% per kill, 2%% per hit", Cy_OverPct(), Cy_OverPct());
        Set_Text(g_setFontSmall, CY_TEAL, 30, 912, CY_LX1, 946, endOk ? "CHROME PERKS" : "CHROME PERKS: NEED END 10", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Fill(30, 950, 42, 962, g_cyBorg ? CY_RED : CY_TEALD);
        snprintf(t, sizeof t, "FULL BORG  %d/%d", filled, total);
        Set_Text(g_setFontSmall, g_cyBorg ? CY_RED : CY_TXT, 52, 938, CY_LX1, 974, t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Text(g_setFontSmall, CY_DIM, 52, 974, CY_LX1, 1006, g_cyBorg ? "30% less damage taken" : endOk ? "fill every slot: -30% damage" : "fill every slot (END 10)", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Fill(30, 1030, 42, 1042, g_cyCO ? CY_RED : endOk && (g_cyBorg || over) ? RGB(200, 120, 40) : CY_TEALD);
        Set_Text(g_setFontSmall, g_cyCO ? CY_RED : CY_TXT, 52, 1018, CY_LX1, 1054, "CHROME OVERLOAD", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Text(g_setFontSmall, CY_DIM, 52, 1054, CY_LX1, 1170, g_cyCO ? "ACTIVE: +50% damage, +50 DR, +50% speed, kills heal 20% and add 3 s" : !endOk ? "needs Endurance 10" : over ? coTxt : g_cyBorg ? "2% chance per hit you land" : "over capacity or full borg: 2% per hit; over capacity: kills too", DT_LEFT | DT_WORDBREAK);
    }
    Set_Fill(24, SET_H - 86, SET_W - 24, SET_H - 83, CY_TEALD);
    Set_Text(g_setFontSmall, CY_DIM, 30, SET_H - 76, SET_W - 30, SET_H - 20, g_cyFromDoc ? "Point + trigger   |   stick: move   |   B: back to talking" : "Point + trigger   |   stick: move   |   B: back", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}
static void Cy_Say(const char *m) { snprintf(g_cyMsg, sizeof g_cyMsg, "%s", m); g_cyMsgUntil = QpcMs() + 5000.0; g_setDirty = true; }
static void Cy_Press(int btn)
{
    const CyImpl *it[8]; int ni = Cy_SlotItems(g_cySlot, it, 8);
    if (!ni) return;
    const CyImpl *c = it[g_cyItem < ni ? g_cyItem : 0];
    Cy_Say(btn == 0 ? Cy_Install(c) : Cy_Remove(c));
    Cy_Refresh();
}
/* where the aiming hand points on the panel (pixels); false off the panel. Also places the cursor. */
static bool Cy_Pointer(float *px, float *py)
{
    TrackedDevicePose_t ps[16]; int h = g_leftHanded ? 0 : 1; bool hit = false;
    g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, ps, 16);
    if (g_handIdx[h] < 16 && ps[g_handIdx[h]].bPoseIsValid) {
        const float (*m)[4] = ps[g_handIdx[h]].mDeviceToAbsoluteTracking.m;
        float o = g_aimPitchOffsetDeg / RAD2DEG, org[3], dir[3], C[3], X[3], Y[3], Z[3];
        for (int k = 0; k < 3; k++) { org[k] = m[k][3]; dir[k] = -m[k][2] * cosf(o) + m[k][1] * sinf(o); }
        for (int k = 0; k < 3; k++) { C[k] = g_setPose.m[k][3]; X[k] = g_setPose.m[k][0]; Y[k] = g_setPose.m[k][1]; Z[k] = g_setPose.m[k][2]; }
        float dn = V3Dot(dir, Z);
        if (dn < -0.05f) {
            float d[3]; V3Sub(d, C, org); float t = V3Dot(d, Z) / dn;
            if (t > 0.02f && t < 4.0f) {
                float P[3]; for (int k = 0; k < 3; k++) P[k] = org[k] + dir[k] * t;
                float q[3]; V3Sub(q, P, C);
                float W = g_setWidthM, H = g_setWidthM * SET_H / SET_W, u = V3Dot(q, X) / W + 0.5f, v = 0.5f - V3Dot(q, Y) / H;
                if (u >= 0 && u <= 1 && v >= 0 && v <= 1) {
                    hit = true; *px = u * SET_W; *py = v * SET_H;
                    HmdMatrix34_t cm = g_setPose; for (int k = 0; k < 3; k++) cm.m[k][3] = P[k] + Z[k] * 0.004f;
                    if (g_setCurOv) { g_vrOverlay->SetOverlayTransformAbsolute(g_setCurOv, ETrackingUniverseOrigin_TrackingUniverseStanding, &cm); if (!g_setCurShown) { g_vrOverlay->ShowOverlay(g_setCurOv); g_setCurShown = true; } }
                }
            }
        }
    }
    if (!hit && g_setCurShown && g_setCurOv) { g_vrOverlay->HideOverlay(g_setCurOv); g_setCurShown = false; }
    return hit;
}
static void Cy_Update(const VRControllerState_t *mv, const VRControllerState_t *am, bool okMv, bool okAm, bool trigEdge, bool aEdge, bool bEdge)
{
    static double navAt = 0; static int navDir = 0, sideDir = 0;
    const CyImpl *it[8]; int ni = Cy_SlotItems(g_cySlot, it, 8);
    float px = 0, py = 0; int hk = 0, hi = -1;
    if (Cy_Pointer(&px, &py)) {
        if (px >= CY_LX0 && px <= CY_LX1 && py >= CY_Y0 && py < CY_Y0 + CY_NSLOT * CY_SLH) { hk = 1; hi = (int)((py - CY_Y0) / CY_SLH); }
        else if (px >= CY_RX0 && px <= CY_RX1 && py >= CY_Y0 && py < CY_Y0 + ni * CY_ITH) { hk = 2; hi = (int)((py - CY_Y0) / CY_ITH); }
        else if (py >= CY_BTNY0 && py <= CY_BTNY1 && px >= CY_RX0 && px <= CY_RX1) { hk = 3; hi = px < (CY_RX0 + CY_RX1) / 2 ? 0 : 1; }
    }
    if (hk != g_cyHoverKind || hi != g_cyHoverIdx) { g_cyHoverKind = hk; g_cyHoverIdx = hi; g_setDirty = true; if (hk) Set_Buzz(250); }
    if (trigEdge && hk) {
        if (hk == 1) { g_cySlot = hi; g_cyItem = 0; g_cyCol = 0; Set_Buzz(700); }
        else if (hk == 2) { g_cyItem = hi; g_cyCol = 1; Set_Buzz(700); }
        else if (hk == 3) { g_cyCol = 2; g_cyBtn = hi; Cy_Press(hi); }
        g_setDirty = true;
    } else if (aEdge || (trigEdge && !hk)) {
        if (g_cyCol == 0) { g_cyCol = 1; g_cyItem = 0; }
        else if (g_cyCol == 1) { g_cyCol = 2; g_cyBtn = Cy_InstIdx(ni ? it[g_cyItem]->id : "") >= 0 ? 1 : 0; }
        else Cy_Press(g_cyBtn);
        g_setDirty = true; Set_Buzz(700);
    }
    float sy = 0, sx = 0;
    if (okMv) { sy = mv->rAxis[0].y; sx = mv->rAxis[0].x; }
    if (okAm && fabsf(am->rAxis[0].y) > fabsf(sy)) sy = am->rAxis[0].y;
    if (okAm && fabsf(am->rAxis[0].x) > fabsf(sx)) sx = am->rAxis[0].x;
    int nd = fabsf(sy) > 0.6f && fabsf(sy) > fabsf(sx) ? (sy > 0 ? -1 : 1) : 0;
    int sd = fabsf(sx) > 0.6f && fabsf(sx) >= fabsf(sy) ? (sx > 0 ? 1 : -1) : 0;
    double now = QpcMs();
    bool step = false;
    if (nd != navDir) { navDir = nd; navAt = now; step = nd != 0; }
    else if (nd && now - navAt > 380.0) { navAt = now - 290.0; step = true; }
    if (step) {
        if (g_cyCol == 0) { g_cySlot = (g_cySlot + nd + CY_NSLOT) % CY_NSLOT; g_cyItem = 0; }
        else if (g_cyCol == 1 && ni) g_cyItem = (g_cyItem + nd + ni) % ni;
        g_setDirty = true; Set_Buzz(300);
    }
    if (sd != sideDir) {
        sideDir = sd;
        if (sd) {
            if (g_cyCol == 2) { if (sd > 0 && g_cyBtn == 0) g_cyBtn = 1; else if (sd < 0 && g_cyBtn == 1) g_cyBtn = 0; else if (sd < 0) g_cyCol = 1; }
            else g_cyCol = sd > 0 ? (g_cyCol + 1 > 2 ? 2 : g_cyCol + 1) : (g_cyCol > 0 ? g_cyCol - 1 : 0);
            g_setDirty = true; Set_Buzz(300);
        }
    }
    if (bEdge) {
        if (g_cyFromDoc) Settings_Close();
        else { g_setPage = SP_MAIN; g_setSel = g_setPageSel[SP_MAIN]; g_setScroll = 0; g_setDirty = true; Set_Buzz(900); }
    }
    {   /* (the caps and capacity shown stay current) */
        static double redraw = 0; if (now > redraw) { redraw = now + 1000.0; Cy_Refresh(); g_setDirty = true; }
    }
}

/* (FNVR settings > Cyberware: the page opened from the settings list - B goes back to it) */
static void Cy_EnterFromSettings(void) { g_cyFromDoc = false; g_cyCol = 0; g_cyMsgUntil = 0; Cy_Refresh(); }
