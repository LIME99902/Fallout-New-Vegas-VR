/* ==== Chromed enemies =================================================================================================
   Included by fnvr_cyber.c (it uses the implant catalogue and the script helpers there).

   Human enemies (characters, not creatures or robots) can carry the same implants you can. Each one is decided the
   first time it fights you: whether it has chrome, how much, and which, scaled by YOUR level - a few at the start,
   most enemies well chromed by the end of the game. Bosses (enemies well above your level) are always chromed and carry
   far more. Their stat implants go in with the game's own ModAV on them (so the game keeps them with the save), and
   what each enemy carries is written into FNVR's co-save so its abilities come back after loading.

   The story's big names (Caesar, Legate Lanius, Motor-Runner, Benny...) are maxed out: every slot filled, always a
   Backup Heart, a full borg's toughness (+30 damage resistance) and over capacity - they can snap into cyberpsychosis.

   Their abilities, in a fight with you:
   - Overclock Core: a burst of speed for 2.5 seconds (to close in or get out of your line of fire), then 10 seconds
     before it can do it again.
   - Berserker Core: below half health, +25 damage resistance and close-up damage for 12 seconds.
   - Ghost Core: optical camo for 8 seconds now and then.
   - Backup Heart: once, below 15% health, back to full.
   - Cyberpsychosis: an enemy with more chrome than its body can handle can snap, just as your Chrome Overload does -
     2% each time it hits something, and on each kill a chance equal to how far over capacity it is. It goes frenzied
     (the game's own "frenzied" aggression: it attacks anyone near it), +50 damage resistance, +50% speed, kills heal it
     and keep it going, for 20 seconds (+3 per kill).
   Boosts that are on when the game is saved are undone again when that save is loaded. */

#define CE_MAX 192
#define CE_MAXIMP 32
#define CE_REC 0x43594245u   /* 'CYBE' */
typedef struct {
    uint32_t refID; uint8_t nImp, boss, over, heartUsed;
    int16_t imp[CE_MAXIMP];                 /* catalogue entries (this session) */
    int cap, load, core;                     /* capacity, chrome load, the core's ability (0 none) */
    double seenAt, abilReady, abilEnd; int abilOn;
    CySavedMod temp[4]; int tempN;           /* an ability's boosts, on now */
    bool psycho; double psychoEnd, psychoRetarget; int aggrPrev;
} CEnemy;
typedef struct {                             /* (as kept in the co-save) */
    uint32_t refID; uint8_t nImp, boss, over, heartUsed; int32_t cap, load;
    char ids[CE_MAXIMP][24];
    int32_t tempN; CySavedMod temp[4]; int32_t psycho, aggrPrev;
} CESave;
static CEnemy g_ce[CE_MAX]; static int g_ceN = 0;
static uint32_t g_ceDone[1024]; static int g_ceDoneN = 0, g_ceDoneHead = 0;   /* (characters already decided - chrome or not) */
static struct { uint32_t refID; CySavedMod temp[4]; int tempN; int aggrPrev; bool psycho; } g_ceUndo[32]; static int g_ceUndoN = 0;
static struct { void *att, *vic; bool kill; } g_ceEv[64]; static volatile LONG g_ceEvN = 0;
static int g_cyPlayerLvl = 1;

static void Ce_Event(void *attacker, void *victim, bool kill)   /* (from xNVSE's events; handled on the next tick) */
{
    LONG i = InterlockedIncrement(&g_ceEvN) - 1;
    if (i < 64) { g_ceEv[i].att = attacker; g_ceEv[i].vic = victim; g_ceEv[i].kill = kill; }
}
static const char *Cy_RefName(char *ref)   /* a character's name (its base form's full name) */
{
    char *base = ref && !IsBadReadPtr(ref + 0x20, 4) ? *(char **)(ref + 0x20) : NULL;
    for (int off = 0xC8; base && off <= 0xE0; off += 4) {
        const char *s = !IsBadReadPtr(base + off, 4) ? *(const char **)(base + off) : NULL;
        if (s && !IsBadStringPtrA(s, 64) && s[0] >= 'A' && s[0] <= 'Z' && strlen(s) < 48) return s;
    }
    return "Someone";
}
/* scripts run on one character: (ref) or (ref, int) - a number back */
static bool Ce_Call(void *scr, char *ref, int nArgs, int a2, double *out)
{
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    if (!scr || !ref || !g_scriptIntf || !g_scriptIntf->CallFunction || !GetPlayer()) return false;
    struct { union { double num; void *form; } u; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    bool ok = nArgs >= 2 ? ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 2, ref, a2)
                         : ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 1, ref);
    if (out) *out = ok && res.type == 1 ? res.u.num : 0.0;
    return ok;
}
static bool Ce_ModAV(char *ref, const char *av, int amt)
{
    static struct { char av[20]; void *scr; bool bad; } cache[40]; static int n = 0;
    int k = 0; for (; k < n; k++) if (!strcmp(cache[k].av, av)) break;
    if (k == n) {
        if (n >= 40) return false;
        snprintf(cache[n].av, sizeof cache[n].av, "%s", av);
        char src[160]; snprintf(src, sizeof src, "ref rW\nint iV\nBegin Function { rW, iV }\nrW.ModAV %s iV\nEnd\n", av);
        cache[n].scr = Cy_Compile(src); cache[n].bad = !cache[n].scr; n++;
    }
    return !cache[k].bad && Ce_Call(cache[k].scr, ref, 2, amt, NULL);
}
static double Ce_Num(int which, char *ref)   /* 0 health fraction, 1 level, 2 dead, 3 aggression */
{
    static void *s[4];
    static const char *const src[4] = {
        "ref rW\nfloat fV\nBegin Function { rW }\nset fV to rW.GetHealthPercentage\nSetFunctionValue fV\nEnd\n",
        "ref rW\nint iV\nBegin Function { rW }\nset iV to rW.GetLevel\nSetFunctionValue iV\nEnd\n",
        "ref rW\nint iV\nBegin Function { rW }\nset iV to rW.GetDead\nSetFunctionValue iV\nEnd\n",
        "ref rW\nint iV\nBegin Function { rW }\nset iV to rW.GetAV Aggression\nSetFunctionValue iV\nEnd\n" };
    if (!s[which]) s[which] = Cy_Compile(src[which]);
    double v = -1; if (!Ce_Call(s[which], ref, 1, 0, &v)) return -1;
    if (which == 0 && v > 1.5) v /= 100.0;
    return v;
}
static void Ce_Heal(char *ref, int pct)
{
    static void *s = NULL;
    if (!s) s = Cy_Compile("ref rW\nint iP\nfloat fC\nfloat fP\nfloat fM\nBegin Function { rW, iP }\nset fC to rW.GetAV Health\nset fP to rW.GetHealthPercentage\nif fP > 1.5\nset fP to fP / 100\nendif\nif fP > 0.01\nset fM to fC / fP * iP / 100\nrW.RestoreAV Health fM\nendif\nEnd\n");
    Ce_Call(s, ref, 2, pct, NULL);
}
static void Ce_SetAggr(char *ref, int v)
{
    static void *s = NULL; if (!s) s = Cy_Compile("ref rW\nint iV\nBegin Function { rW, iV }\nrW.SetAV Aggression iV\nEnd\n");
    Ce_Call(s, ref, 2, v, NULL);
}
static void Ce_AttackNearest(char *ref)   /* (a cyberpsycho goes for whoever is closest) */
{
    static void *s = NULL;
    if (!s) s = Cy_Compile("ref rW\nref rT\nref rBest\nfloat fD\nfloat fBest\nBegin Function { rW }\nset fBest to 3000\nset rT to GetFirstRef 200 1\nwhile rT != 0\nif rT != rW\nif rT.GetDead == 0\nset fD to rW.GetDistance rT\nif fD < fBest\nset fBest to fD\nset rBest to rT\nendif\nendif\nendif\nset rT to GetNextRef\nloop\nif rBest\nrW.StartCombat rBest\nendif\nEnd\n");
    Ce_Call(s, ref, 1, 0, NULL);
}
static char *Ce_Ref(const CEnemy *e)
{
    char *r = (char *)LookupForm(e->refID);
    return r && !IsBadReadPtr(r, 0x80) && *(uint8_t *)(r + 4) == 0x3B ? r : NULL;
}
static CEnemy *Ce_Find(uint32_t id) { for (int i = 0; i < g_ceN; i++) if (g_ce[i].refID == id) return &g_ce[i]; return NULL; }
static bool Ce_Decided(uint32_t id) { for (int i = 0; i < g_ceDoneN; i++) if (g_ceDone[i] == id) return true; return false; }
static void Ce_MarkDecided(uint32_t id) { g_ceDone[g_ceDoneHead] = id; g_ceDoneHead = (g_ceDoneHead + 1) % 1024; if (g_ceDoneN < 1024) g_ceDoneN++; }
static void Ce_TempOn(CEnemy *e, char *ref, const char *av, int amt)
{
    if (e->tempN < 4 && Ce_ModAV(ref, av, amt)) { snprintf(e->temp[e->tempN].av, sizeof e->temp[0].av, "%s", av); e->temp[e->tempN].amt = amt; e->tempN++; }
}
static void Ce_TempOff(CEnemy *e, char *ref)
{
    if (ref) for (int i = 0; i < e->tempN; i++) Ce_ModAV(ref, e->temp[i].av, -e->temp[i].amt);
    e->tempN = 0; e->abilOn = 0;
}

/* the story's big names: maxed-out chrome whatever your level (their in-game names, exactly) */
static const char *const g_ceNamed[] = { "Caesar", "Legate Lanius", "Vulpes Inculta", "Motor-Runner", "Benny", "Cook-Cook", "Driver Nephi",
    "Jessup", "McMurphy", "Papa Khan", "General Lee Oliver", "Chief Hanlon", "Elijah", "Ulysses", "Salt-Upon-Wounds", "Joshua Graham", "Big Sal", "Nero", "Dog", NULL };
static bool Ce_IsNamedBoss(const char *nm) { for (int i = 0; g_ceNamed[i]; i++) if (!_stricmp(nm, g_ceNamed[i])) return true; return false; }
/* the first fight with you: does this one have chrome, and what */
static void Ce_Decide(char *ref)
{
    uint32_t id = *(uint32_t *)(ref + 0x0C);
    Ce_MarkDecided(id);
    int plv = g_cyPlayerLvl > 0 ? g_cyPlayerLvl : 1, lvl = (int)Ce_Num(1, ref); if (lvl < 1) lvl = 1;
    bool named = Ce_IsNamedBoss(Cy_RefName(ref));
    bool boss = named || lvl >= plv + 5 || (plv >= 8 && lvl * 10 >= plv * 13);
    int chance = boss ? 100 : (int)fminf(90.0f, 15.0f + plv * 2.5f);
    if (rand() % 100 >= chance || g_ceN >= CE_MAX) return;
    int want = named ? CE_MAXIMP : boss ? 3 + plv / 4 : 1 + plv / 7 + rand() % 2;
    if (want > CE_MAXIMP) want = CE_MAXIMP;
    CEnemy *e = &g_ce[g_ceN]; memset(e, 0, sizeof *e);
    e->refID = id; e->boss = boss; e->cap = 16 + 20 + lvl;
    int order[CY_NCAT]; for (int i = 0; i < CY_NCAT; i++) order[i] = i;
    for (int i = CY_NCAT - 1; i > 0; i--) { int j = rand() % (i + 1), t = order[i]; order[i] = order[j]; order[j] = t; }
    if (named) for (int i = 0; i < CY_NCAT; i++) if (g_cyCat[order[i]].special == CYS_HEART) { int t = order[0]; order[0] = order[i]; order[i] = t; break; }   /* (always a Backup Heart) */
    int used[CY_NSLOT] = { 0 }; bool blade = false;
    for (int k = 0; k < CY_NCAT && e->nImp < want; k++) {
        const CyImpl *c = &g_cyCat[order[k]];
        if (c->special == CYS_DJUMP || c->special == CYS_NOFALL) continue;   /* (nothing for the AI to do with these) */
        if (used[c->slot] >= g_cySlotMax[c->slot]) continue;
        bool isBlade = strstr(c->name, "Arm Blades") != NULL; if (isBlade && blade) continue;
        used[c->slot]++; if (isBlade) blade = true;
        e->imp[e->nImp++] = (int16_t)order[k]; e->load += c->load;
        if (c->slot == CY_CORE) e->core = c->special;
        for (int m = 0; m < CY_MAXMOD && c->mod[m].av; m++) Ce_ModAV(ref, c->mod[m].av, c->mod[m].amt);
    }
    if (named) {   /* maxed out: a full borg's toughness, and too much chrome for any body */
        Ce_ModAV(ref, "DamageResist", 30);
        if (e->load <= e->cap) e->load = e->cap * 2;
    }
    else if (boss && e->load <= e->cap && rand() % 100 < 50) e->load = e->cap + e->cap / 4;   /* (half the bosses are pushing it) */
    e->over = e->load > e->cap;
    g_ceN++;
    char list[1200] = ""; for (int i = 0; i < e->nImp; i++) { strncat(list, g_cyCat[e->imp[i]].name, sizeof list - strlen(list) - 3); if (i + 1 < e->nImp) strncat(list, ", ", sizeof list - strlen(list) - 1); }
    if (named) { char m[96]; snprintf(m, sizeof m, "%s is chromed to the limit", Cy_RefName(ref)); UIMessage(m); }
    Log("Chromed enemy: %s (level %d%s) - %d implants%s: %s", Cy_RefName(ref), lvl, named ? ", MAXED-OUT BOSS" : boss ? ", BOSS" : "", e->nImp, e->over ? ", OVER CAPACITY" : "", list);
}
static int Ce_OverPct(const CEnemy *e) { if (e->load <= e->cap || e->cap <= 0) return 0; int p = (e->load - e->cap) * 100 / e->cap; return p > 100 ? 100 : p < 1 ? 1 : p; }
static void Ce_PsychoStart(CEnemy *e, char *ref)
{
    if (e->psycho) return;
    e->psycho = true; e->psychoEnd = QpcMs() + 20000.0; e->psychoRetarget = 0;
    if (e->abilOn) Ce_TempOff(e, ref);
    int a = (int)Ce_Num(3, ref); e->aggrPrev = a >= 0 && a <= 3 ? a : 1;
    Ce_TempOn(e, ref, "DamageResist", 50); Ce_TempOn(e, ref, "SpeedMult", 50);
    Ce_SetAggr(ref, 3);   /* frenzied: attacks anyone */
    char m[96]; snprintf(m, sizeof m, "%s has gone cyberpsycho!", Cy_RefName(ref)); UIMessage(m);
    Log("Chromed enemy: %s went cyberpsycho", Cy_RefName(ref));
}
static void Ce_PsychoEnd(CEnemy *e, char *ref)
{
    if (!e->psycho) return;
    Ce_TempOff(e, ref); e->psycho = false;
    if (ref) Ce_SetAggr(ref, e->aggrPrev);
}
/* the characters fighting you (one at a time: the next after rLast) */
static char *Ce_NextFoe(char *last)
{
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    static void *s = NULL;
    if (!s) s = Cy_Compile("ref rLast\nref rW\nref rOut\nint iSeen\nBegin Function { rLast }\nset iSeen to 0\nif rLast == 0\nset iSeen to 1\nendif\nset rW to GetFirstRef 200 1\nwhile rW != 0\nif iSeen\nif rW.GetDead == 0\nif rW.GetCombatTarget == player\nset rOut to rW\nbreak\nelseif rW.IsInCombat\nif rW.GetShouldAttack player\nset rOut to rW\nbreak\nendif\nendif\nendif\nelseif rW == rLast\nset iSeen to 1\nendif\nset rW to GetNextRef\nloop\nSetFunctionValue rOut\nEnd\n");
    if (!s || !g_scriptIntf || !g_scriptIntf->CallFunction) return NULL;
    struct { union { double num; void *form; } u; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    if (!((CallFn_t)g_scriptIntf->CallFunction)(s, GetPlayer(), NULL, &res, 1, last) || res.type != 2) return NULL;
    return (char *)res.u.form;
}
static void Ce_Tick(double now)
{
    if (!g_cyEnemies) return;
    for (int i = 0; i < g_ceUndoN; i++) {   /* (a save made while an enemy's boost was on: undone once it's loaded) */
        char *ref = (char *)LookupForm(g_ceUndo[i].refID);
        if (ref && !IsBadReadPtr(ref, 0x80)) {
            for (int k = 0; k < g_ceUndo[i].tempN; k++) Ce_ModAV(ref, g_ceUndo[i].temp[k].av, -g_ceUndo[i].temp[k].amt);
            if (g_ceUndo[i].psycho) Ce_SetAggr(ref, g_ceUndo[i].aggrPrev);
        }
    }
    g_ceUndoN = 0;
    static double nextScan = 0;
    if (now > nextScan) {   /* who's fighting you: new ones are decided, chromed ones marked as in the fight */
        nextScan = now + 1500.0;
        char *r = NULL; int guard = 0;
        while ((r = Ce_NextFoe(r)) != NULL && guard++ < 40) {
            if (IsBadReadPtr(r, 0x80) || *(uint8_t *)(r + 4) != 0x3B) continue;   /* (characters only - not creatures or robots) */
            uint32_t id = *(uint32_t *)(r + 0x0C);
            if (!Ce_Decided(id)) Ce_Decide(r);
            CEnemy *e = Ce_Find(id); if (e) e->seenAt = now;
        }
    }
    LONG nev = InterlockedExchange(&g_ceEvN, 0); if (nev > 64) nev = 64;
    for (LONG k = 0; k < nev; k++) {   /* hits and kills by chromed enemies */
        char *att = (char *)g_ceEv[k].att; if (!att || IsBadReadPtr(att, 0x80) || *(uint8_t *)(att + 4) != 0x3B) continue;
        CEnemy *e = Ce_Find(*(uint32_t *)(att + 0x0C)); if (!e) continue;
        if (e->psycho) { if (g_ceEv[k].kill) { e->psychoEnd += 3000.0; Ce_Heal(att, 20); } continue; }
        if (!e->over) continue;
        int pct = g_ceEv[k].kill ? Ce_OverPct(e) : 2;
        if (rand() % 100 < pct) Ce_PsychoStart(e, att);
    }
    static double nextAbil = 0;
    if (now < nextAbil) return;
    nextAbil = now + 500.0;
    for (int i = 0; i < g_ceN; i++) {
        CEnemy *e = &g_ce[i];
        bool fighting = now - e->seenAt < 4000.0;
        if (!fighting && !e->abilOn && !e->psycho) continue;
        char *ref = Ce_Ref(e);
        if (!ref || Ce_Num(2, ref) > 0.5) { if (e->abilOn) Ce_TempOff(e, ref); if (e->psycho) Ce_PsychoEnd(e, ref); continue; }
        if (e->psycho) {
            if (now > e->psychoEnd) { Ce_PsychoEnd(e, ref); Log("Chromed enemy: %s came out of it", Cy_RefName(ref)); }
            else if (now > e->psychoRetarget) { e->psychoRetarget = now + 3000.0; Ce_AttackNearest(ref); }
            continue;
        }
        if (e->abilOn && now > e->abilEnd) Ce_TempOff(e, ref);
        double hp = -1;
        bool hasHeart = false; for (int k = 0; k < e->nImp; k++) if (g_cyCat[e->imp[k]].special == CYS_HEART) hasHeart = true;
        if ((hasHeart && !e->heartUsed) || e->core == CYS_BERSERK) hp = Ce_Num(0, ref);
        if (hasHeart && !e->heartUsed && hp > 0.0 && hp < 0.15) {
            e->heartUsed = 1; Ce_Heal(ref, 100);
            char m[96]; snprintf(m, sizeof m, "%s's backup heart kicked in", Cy_RefName(ref)); UIMessage(m);
        }
        if (!fighting || e->abilOn || now < e->abilReady) continue;
        switch (e->core) {
        case CYS_SLOWMO:   /* a burst of speed - to close in or get out of your sights */
            if (rand() % 100 < 40) { Ce_TempOn(e, ref, "SpeedMult", 200); e->abilOn = CYS_SLOWMO; e->abilEnd = now + 2500.0; e->abilReady = e->abilEnd + 10000.0; }
            break;
        case CYS_BERSERK:
            if (hp > 0.0 && hp < 0.5) { Ce_TempOn(e, ref, "DamageResist", 25); Ce_TempOn(e, ref, "MeleeDamage", 25); Ce_TempOn(e, ref, "UnarmedDamage", 25);
                e->abilOn = CYS_BERSERK; e->abilEnd = now + 12000.0; e->abilReady = e->abilEnd + 45000.0; }
            break;
        case CYS_CAMO:
            if (rand() % 100 < 25) { Ce_TempOn(e, ref, "Chameleon", 1); e->abilOn = CYS_CAMO; e->abilEnd = now + 8000.0; e->abilReady = e->abilEnd + 30000.0; }
            break;
        default: break;
        }
    }
}
static void Ce_Reset(void)
{   /* (a game left or loaded: nothing undone by hand - the game's own values go with it) */
    g_ceN = 0; g_ceDoneN = 0; g_ceDoneHead = 0; g_ceUndoN = 0; g_ceEvN = 0;
}
static void Ce_Save(void)
{
    if (!g_ser || !g_ser->OpenRecord(CE_REC, 2)) return;
    int32_t nd = g_ceDoneN, n = g_ceN;
    g_ser->WriteRecordData(&nd, 4);
    for (int i = 0; i < nd; i++) g_ser->WriteRecordData(&g_ceDone[i], 4);
    g_ser->WriteRecordData(&n, 4);
    for (int i = 0; i < n; i++) {
        CEnemy *e = &g_ce[i]; CESave s; memset(&s, 0, sizeof s);
        s.refID = e->refID; s.nImp = e->nImp; s.boss = e->boss; s.over = e->over; s.heartUsed = e->heartUsed; s.cap = e->cap; s.load = e->load;
        for (int k = 0; k < e->nImp; k++) snprintf(s.ids[k], sizeof s.ids[k], "%s", g_cyCat[e->imp[k]].id);
        s.tempN = e->tempN; memcpy(s.temp, e->temp, sizeof s.temp); s.psycho = e->psycho; s.aggrPrev = e->aggrPrev;
        g_ser->WriteRecordData(&s, sizeof s);
    }
}
static void Ce_Load(uint32_t ver)
{
    if (ver != 2) return;   /* (v1 held 12 implants per enemy: those enemies are simply decided again) */
    int32_t nd = 0, n = 0;
    if (g_ser->ReadRecordData(&nd, 4) != 4 || nd < 0 || nd > 1024) return;
    for (int i = 0; i < nd; i++) { uint32_t id = 0, nid = 0; if (g_ser->ReadRecordData(&id, 4) != 4) return; if (g_ser->ResolveRefID && g_ser->ResolveRefID(id, &nid)) Ce_MarkDecided(nid); }
    if (g_ser->ReadRecordData(&n, 4) != 4 || n < 0 || n > CE_MAX) return;
    for (int i = 0; i < n; i++) {
        CESave s; if (g_ser->ReadRecordData(&s, sizeof s) != sizeof s) return;
        uint32_t nid = 0; if (!g_ser->ResolveRefID || !g_ser->ResolveRefID(s.refID, &nid)) continue;
        if (s.tempN > 0 || s.psycho) {   /* (boosts on at the save: undone after loading) */
            if (g_ceUndoN < 32) { g_ceUndo[g_ceUndoN].refID = nid; g_ceUndo[g_ceUndoN].tempN = s.tempN > 4 ? 4 : s.tempN; memcpy(g_ceUndo[g_ceUndoN].temp, s.temp, sizeof s.temp);
                for (int k = 0; k < 4; k++) g_ceUndo[g_ceUndoN].temp[k].av[19] = 0;
                g_ceUndo[g_ceUndoN].psycho = s.psycho != 0; g_ceUndo[g_ceUndoN].aggrPrev = s.aggrPrev; g_ceUndoN++; }
        }
        if (g_ceN >= CE_MAX) continue;
        CEnemy *e = &g_ce[g_ceN]; memset(e, 0, sizeof *e);
        e->refID = nid; e->boss = s.boss; e->over = s.over; e->heartUsed = s.heartUsed; e->cap = s.cap; e->load = s.load;
        for (int k = 0; k < s.nImp && k < CE_MAXIMP; k++) {
            s.ids[k][23] = 0;
            for (int c = 0; c < CY_NCAT; c++) if (!strcmp(g_cyCat[c].id, s.ids[k])) { e->imp[e->nImp++] = (int16_t)c; if (g_cyCat[c].slot == CY_CORE) e->core = g_cyCat[c].special; break; }
        }
        g_ceN++;
    }
    Log("Chromed enemies: %d decided, %d with chrome", g_ceDoneN, g_ceN);
}
