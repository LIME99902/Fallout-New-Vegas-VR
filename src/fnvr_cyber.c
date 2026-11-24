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
enum { CYS_NONE, CYS_HEART, CYS_SLOWMO, CYS_BERSERK, CYS_CAMO, CYS_NOFALL, CYS_DJUMP, CYS_MECH, CYS_LAUNCH, CYS_DASH, CYS_RAM, CYS_BLOOD, CYS_HACK, CYS_EYES };
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
    { "core_deck1",     CY_CORE, "Brain Dancer Deck Mk.1", 4000, 6, CYS_HACK, { { 0 } },
      "A netrunner's cyberdeck: quickhack people and robots (needs Cyber Eyes). 5 RAM, refills 1 RAM every 2 seconds. Scan (left grip + left trigger), point your gun hand, right stick picks, right trigger uploads: Reboot Optics 3, Weapon Glitch 3, Cyberpsychosis 5, Cyberware Malfunction 4 (5 s each), Rogue AI (a full deck: kills outright)." },
    { "core_deck2",     CY_CORE, "Brain Dancer Deck Mk.2", 6000, 7, CYS_HACK, { { 0 } },
      "A netrunner's cyberdeck: quickhack people and robots (needs Cyber Eyes). 7 RAM, refills 1 RAM every 2 seconds. Scan (left grip + left trigger), point your gun hand, right stick picks, right trigger uploads: Reboot Optics 3, Weapon Glitch 3, Cyberpsychosis 5, Cyberware Malfunction 4 (5 s each), Rogue AI (a full deck: kills outright)." },
    { "core_deck3",     CY_CORE, "Brain Dancer Deck Mk.3", 8500, 8, CYS_HACK, { { 0 } },
      "A netrunner's cyberdeck: quickhack people and robots (needs Cyber Eyes). 10 RAM, refills 1 RAM every 2 seconds. Scan (left grip + left trigger), point your gun hand, right stick picks, right trigger uploads: Reboot Optics 3, Weapon Glitch 3, Cyberpsychosis 5, Cyberware Malfunction 4 (5 s each), Rogue AI (a full deck: kills outright)." },
    { "core_deck4",     CY_CORE, "Brain Dancer Deck Mk.4", 12000, 9, CYS_HACK, { { 0 } },
      "A netrunner's cyberdeck: quickhack people and robots (needs Cyber Eyes). 15 RAM, refills 1 RAM every 2 seconds. Scan (left grip + left trigger), point your gun hand, right stick picks, right trigger uploads: Reboot Optics 3, Weapon Glitch 3, Cyberpsychosis 5, Cyberware Malfunction 4 (5 s each), Rogue AI (a full deck: kills outright)." },
    { "core_deck5",     CY_CORE, "Brain Dancer Deck Mk.5", 16000, 10, CYS_HACK, { { 0 } },
      "A netrunner's cyberdeck: quickhack people and robots (needs Cyber Eyes). 20 RAM, refills 5 RAM a second. Scan (left grip + left trigger), point your gun hand, right stick picks, right trigger uploads: Reboot Optics 3, Weapon Glitch 3, Cyberpsychosis 5, Cyberware Malfunction 4 (5 s each), Rogue AI (a full deck: kills outright)." },
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
    { "optics_cyber",   CY_OPTICS, "Cyber Eyes", 3000, 3, CYS_EYES, { { "Perception", 1 } },
      "Full optical replacements with a scanning overlay. Hold the left grip and left trigger: everyone around you is outlined, and whoever you point your gun hand at shows their health. Needed to quickhack with a Brain Dancer Deck." },
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
    { "arm_mech",       CY_ARMS, "Mech Arms", 9000, 9, CYS_MECH, { { "Strength", 4 }, { "UnarmedDamage", 40 } },
      "Heavy hydraulic arms. +4 Strength, +40 unarmed damage per fist. Grab someone to knock them off their feet and throw them (robots, deathclaws and other giants need Strength 10); grab with both hands and pull to rip a limb off; a punch to the chest sends them flying. (Their own bigger arm model comes in a later update.)" },
    { "arm_hydraulic",  CY_ARMS, "Hydraulic Forearms", 3000, 5, CYS_NONE, { { "MeleeDamage", 10 } },
      "Hydraulic rams along both forearms. Every swing of a melee weapon lands harder (+10 melee damage)." },
    /* one arm at a time (left and right can differ - a mantis blade on one, a Mech Arm on the other): half the price and
       capacity of a pair, and the fist's own damage goes to that hand only (FNVR deals it where the blow lands) */
    { "arm_kinetic_l",  CY_ARMS, "Arm Blade: Kinetic (left)", 2500, 4, CYS_NONE, { { 0 } }, "A serrated blade rising out of the left forearm: +15 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_kinetic_r",  CY_ARMS, "Arm Blade: Kinetic (right)", 2500, 4, CYS_NONE, { { 0 } }, "A serrated blade rising out of the right forearm: +15 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_thermal_l",  CY_ARMS, "Arm Blade: Thermal (left)", 3250, 4, CYS_NONE, { { "FireResist", 5 } }, "A heated blade in the left forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_thermal_r",  CY_ARMS, "Arm Blade: Thermal (right)", 3250, 4, CYS_NONE, { { "FireResist", 5 } }, "A heated blade in the right forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_shock_l",    CY_ARMS, "Arm Blade: Shock (left)", 3250, 4, CYS_NONE, { { 0 } }, "An arcing blade in the left forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_shock_r",    CY_ARMS, "Arm Blade: Shock (right)", 3250, 4, CYS_NONE, { { 0 } }, "An arcing blade in the right forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_toxin_l",    CY_ARMS, "Arm Blade: Toxin (left)", 3250, 4, CYS_NONE, { { "PoisonResist", 5 } }, "A venom blade in the left forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_quickie_l",  CY_ARMS, "Quickie (left) - LEGENDARY", 6000, 5, CYS_NONE, { { "EnergyResist", 5 } }, "A legendary mantis blade with a netrunner's daemon in the edge: +20 damage to that hand's blows, and a 30% chance on every hit to upload Cyberware Malfunction - their chrome shuts off for 5 s, a jolt shocks them and stuns them for a second. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_quickie_r",  CY_ARMS, "Quickie (right) - LEGENDARY", 6000, 5, CYS_NONE, { { "EnergyResist", 5 } }, "A legendary mantis blade with a netrunner's daemon in the edge: +20 damage to that hand's blows, and a 30% chance on every hit to upload Cyberware Malfunction - their chrome shuts off for 5 s, a jolt shocks them and stuns them for a second. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_toxin_r",    CY_ARMS, "Arm Blade: Toxin (right)", 3250, 4, CYS_NONE, { { "PoisonResist", 5 } }, "A venom blade in the right forearm: +12 damage to that hand's blows. Praying Mantis (AGI 10, END 10): every hit stacks a bleed that heals you and bleeds them harder." },
    { "arm_mech_l",     CY_ARMS, "Mech Arm (left)", 4500, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic left arm: +2 Strength, +40 damage to that fist (twice that at Unarmed 100). Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast. It grabs people off their feet and throws them, a punch with it to the chest sends them flying, and with both hands on someone it rips a limb off. Robots, deathclaws and giants need Strength 10." },
    { "arm_mechthunder_l", CY_ARMS, "Thunder Cannon (left) - LEGENDARY", 9000, 6, CYS_MECH, { { "Strength", 2 } },
      "A legendary Mech Arm with a capacitor bank in the forearm: +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and a 20% chance on every hit to discharge - 100 electric damage to them and to every enemy within 6 m. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast. Everything a Mech Arm does: grab and throw, chest punches, ripping limbs off." },
    { "arm_mechthunder_r", CY_ARMS, "Thunder Cannon (right) - LEGENDARY", 9000, 6, CYS_MECH, { { "Strength", 2 } },
      "A legendary Mech Arm with a capacitor bank in the forearm: +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and a 20% chance on every hit to discharge - 100 electric damage to them and to every enemy within 6 m. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast. Everything a Mech Arm does: grab and throw, chest punches, ripping limbs off." },
    { "arm_mech_r",     CY_ARMS, "Mech Arm (right)", 4500, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic right arm: +2 Strength, +40 damage to that fist (twice that at Unarmed 100). Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast. It grabs people off their feet and throws them, a punch with it to the chest sends them flying, and with both hands on someone it rips a limb off. Robots, deathclaws and giants need Strength 10." },
    { "arm_mechblade_l", CY_ARMS, "Mech Arm: Bladed (left)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic left arm with razor edges along the knuckles: hits leave them bleeding. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechblade_r", CY_ARMS, "Mech Arm: Bladed (right)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic right arm with razor edges along the knuckles: hits leave them bleeding. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechthermal_l", CY_ARMS, "Mech Arm: Thermal (left)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic left arm with heated knuckle plates: hits set them burning. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechthermal_r", CY_ARMS, "Mech Arm: Thermal (right)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic right arm with heated knuckle plates: hits set them burning. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechshock_l", CY_ARMS, "Mech Arm: Electric (left)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic left arm with arcing knuckle plates: hits shock them. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechshock_r", CY_ARMS, "Mech Arm: Electric (right)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic right arm with arcing knuckle plates: hits shock them. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechtoxic_l", CY_ARMS, "Mech Arm: Toxic (left)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic left arm with venom-fed knuckle spikes: hits poison them. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_mechtoxic_r", CY_ARMS, "Mech Arm: Toxic (right)", 5000, 5, CYS_MECH, { { "Strength", 2 } },
      "A heavy hydraulic right arm with venom-fed knuckle spikes: hits poison them. +2 Strength, +40 damage to that fist (twice that at Unarmed 100), and all a Mech Arm does: grab and throw people, chest punches that send them flying, ripping limbs off with both hands. Cyber Gorilla (STR 10, END 10): up to +100% from fast swings and moving fast." },
    { "arm_launchjack_l", CY_ARMS, "Jack's Favorite (left) - LEGENDARY", 10000, 6, CYS_LAUNCH, { { 0 } },
      "A legendary left-forearm launcher rebuilt into a laser: make a fist (grip) and hold the trigger - a beam at 1200 shots a minute that sets what it hits burning, for up to 6 seconds of fire, then a 6 second recharge. Every bolt has a 0.05% chance to upload Rogue AI into whoever it's aimed at - a person or robot dies outright." },
    { "arm_launchjack_r", CY_ARMS, "Jack's Favorite (right) - LEGENDARY", 10000, 6, CYS_LAUNCH, { { 0 } },
      "A legendary right-forearm launcher rebuilt into a laser: make a fist (grip) and hold the trigger - a beam at 1200 shots a minute that sets what it hits burning, for up to 6 seconds of fire, then a 6 second recharge. Every bolt has a 0.05% chance to upload Rogue AI into whoever it's aimed at - a person or robot dies outright." },
    { "arm_launchfrag_l", CY_ARMS, "Launcher: Frag (left)", 4000, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the left forearm: explosive mini-rockets. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchfrag_r", CY_ARMS, "Launcher: Frag (right)", 4000, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the right forearm: explosive mini-rockets. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchthermal_l", CY_ARMS, "Launcher: Thermal (left)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the left forearm: incendiary mini-rockets: the blast sets everyone in it burning. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchthermal_r", CY_ARMS, "Launcher: Thermal (right)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the right forearm: incendiary mini-rockets: the blast sets everyone in it burning. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchshock_l", CY_ARMS, "Launcher: Electric (left)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the left forearm: shock mini-rockets: the blast shocks everyone in it and stuns robots. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchshock_r", CY_ARMS, "Launcher: Electric (right)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the right forearm: shock mini-rockets: the blast shocks everyone in it and stuns robots. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchtoxic_l", CY_ARMS, "Launcher: Toxic (left)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the left forearm: gas mini-rockets: the blast poisons everyone in it. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchtoxic_r", CY_ARMS, "Launcher: Toxic (right)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the right forearm: gas mini-rockets: the blast poisons everyone in it. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchshrapnel_l", CY_ARMS, "Launcher: Shrapnel (left)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the left forearm: shrapnel mini-rockets: everyone in the blast bleeds. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_launchshrapnel_r", CY_ARMS, "Launcher: Shrapnel (right)", 4500, 5, CYS_LAUNCH, { { 0 } },
      "A rocket launcher that folds out of the right forearm: shrapnel mini-rockets: everyone in the blast bleeds. Make a fist (grip), then pull the trigger: 3 rockets, one per pull, 70 explosive damage each (twice that at Explosives 100); it refills itself in 20 seconds (5 at Explosives 100). Missile Barrage (LCK 10, END 10): 5 rockets, kills can refund them." },
    { "arm_grapple_l", CY_ARMS, "Grapple Arm (left)", 1000, 2, CYS_NONE, { { 0 } },
      "A grappling line that fires out of the left wrist. Make a fist (grip), then pull the trigger: it pulls you to where your hand points, or hooks a person and drags them to you - they arrive taking 50 damage and bleeding." },
    { "arm_grapple_r", CY_ARMS, "Grapple Arm (right)", 1000, 2, CYS_NONE, { { 0 } },
      "A grappling line that fires out of the right wrist. Make a fist (grip), then pull the trigger: it pulls you to where your hand points, or hooks a person and drags them to you - they arrive taking 50 damage and bleeding." },
    { "arm_ram",        CY_ARMS, "More RAM, Please", 3000, 3, CYS_RAM, { { 0 } },
      "Extra memory banks wired along the forearm. Every kill takes 1 second off your Neural Core ability's cooldown." },
    { "arm_blood",      CY_ARMS, "Blood Driver", 3500, 4, CYS_BLOOD, { { 0 } },
      "Micro-pumps in the forearm drink from every blow. Each melee or unarmed hit that lands heals you 3% of your health." },
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
    { "leg_dash",       CY_LEGS, "Dash Actuators", 5000, 5, CYS_DASH, { { 0 } },
      "Explosive-burst pistons in your thighs. Hold the right grip and press jump to dash about 5 feet the way your left stick points (forward with the stick at rest), on the ground or in the air. Each dash costs 20 Action Points." },
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
static volatile LONG g_cyHits = 0, g_cyKills = 0, g_cyKillsL = 0;                  /* (from xNVSE's events: hits and kills by you, not handled yet) */
static volatile bool g_cyJumpEdge = false;
static bool g_cyBrainOk = false;   /* (Cyber Brained: Intelligence 10, Endurance 10) */
static bool g_cyBorg = false, g_cyCO = false; static double g_cyCOEnd = 0, g_cyCOReady = 0;
static bool g_cyAtDoc = false;                                     /* talking to the ripperdoc right now */
static volatile bool g_cyActivate = false;
static volatile int g_cyHackStep = 0; static volatile bool g_cyHackFire = false;   /* (scanning: the right stick steps through the quickhacks, the right trigger uploads) */
static void Cy_HackOpen(void);                         /* the two-squeeze gesture happened */
static NVSESerializationInterface *g_ser = NULL;
static int g_cyCapsShown = 0, g_cyCapShown = 36, g_cyEnd = 0, g_cyPlvShown = 1;
static void Ce_Event(void *attacker, void *victim, bool kill);
#define CY_PERK_END 10   /* Endurance needed for the chrome perks (any level) */                   /* (read on the game's thread, shown by the panel) */
static void Cyber_Open(bool atDoc);

/* the arm weapons: blades and Mech Arms. Those with _l / _r go on one arm; the old pairs (arm_kinetic, arm_mech...) are
   no longer sold but still work in saves that have them (and enemies carry them) */
static bool Cy_ArmWeapon(const char *id) { return !strncmp(id, "arm_kinetic", 11) || !strncmp(id, "arm_thermal", 11) || !strncmp(id, "arm_shock", 9) || !strncmp(id, "arm_toxin", 9) || !strncmp(id, "arm_mech", 8) || !strncmp(id, "arm_launch", 10) || !strncmp(id, "arm_grapple", 11) || !strncmp(id, "arm_quickie", 11); }
static int Cy_ArmSide(const char *id)   /* 0 left, 1 right, 2 both (the old pairs), -1 not an arm weapon */
{
    if (!Cy_ArmWeapon(id)) return -1;
    size_t n = strlen(id); return n > 2 && id[n - 2] == '_' ? (id[n - 1] == 'l' ? 0 : id[n - 1] == 'r' ? 1 : 2) : 2;
}
static float Cy_ArmBonus(const char *id)   /* (a one-arm weapon's damage to that hand's blows) */
{
    return !strncmp(id, "arm_mech", 8) ? 40.0f : !strncmp(id, "arm_quickie", 11) ? 20.0f : !strncmp(id, "arm_kinetic", 11) ? 15.0f : 12.0f;
}
static int Cy_ArmFx(const char *id)   /* an arm weapon's effect on what it hits: 0 none, 1 burning, 2 shock, 3 poison, 4 bleeding */
{
    if (strstr(id, "thermal")) return 1;
    if (strstr(id, "shock")) return 2;
    if (strstr(id, "toxi")) return 3;   /* (toxin blades, toxic fists) */
    if (!strncmp(id, "arm_mechblade", 13)) return 4;
    return 0;
}
static int Cy_LaunchKind(const char *id)   /* 0 frag, 1 thermal, 2 electric, 3 toxic, 4 shrapnel */
{
    return strstr(id, "jack") ? 5 : strstr(id, "thermal") ? 1 : strstr(id, "shock") ? 2 : strstr(id, "toxic") ? 3 : strstr(id, "shrapnel") ? 4 : 0;
}
static const CyImpl *Cy_Find(const char *id) { for (int i = 0; i < CY_NCAT; i++) if (!strcmp(g_cyCat[i].id, id)) return &g_cyCat[i]; return NULL; }
static int Cy_InstIdx(const char *id) { for (int i = 0; i < g_cyN; i++) if (!strcmp(g_cyInst[i].id, id)) return i; return -1; }
static int Cy_SlotUsed(int slot) { int n = 0; for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c && c->slot == slot) n++; } return n; }
static int Cy_LoadUsed(void) { int n = 0; for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c) n += c->load; } return n; }
static bool Cy_Has(int special) { for (int i = 0; i < g_cyN; i++) { const CyImpl *c = Cy_Find(g_cyInst[i].id); if (c && c->special == special) return true; } return false; }
static bool Cy_HasDash(void) { return Cy_Has(CYS_DASH); }
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
static void Cy_Buzz(int us) { for (int h = 0; h < 2; h++) if (g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, (unsigned short)us); }
static bool Cy_CanInstallHere(void) { return g_cyAtDoc || g_cyAnywhere; }
static const char *Cy_Install(const CyImpl *c)
{
    static char why[160];
    if (!Cy_CanInstallHere()) return "Only a ripperdoc can do that - see Hunter at the New Vegas Medical Clinic";
    if (Cy_InstIdx(c->id) >= 0) return "Already installed";
    if (g_cyN >= CY_MAXINST) return "No room for more implants";
    {   int sd = Cy_ArmSide(c->id);   /* one arm weapon per arm (blade or Mech Arm) */
        if (sd >= 0) for (int i = 0; i < g_cyN; i++) { int so = Cy_ArmSide(g_cyInst[i].id); const CyImpl *o = Cy_Find(g_cyInst[i].id);
            if (so >= 0 && o && (so == sd || so == 2 || sd == 2)) { snprintf(why, sizeof why, "That arm already has %s - one arm weapon per arm", o->name); return why; } } }
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
/* everyone who dies is noted (handled on the next tick: a Fiend gets a Fiend Ear to loot - FNVVRMC - Hunter.esp) */
static void *volatile g_cyDead[32]; static volatile LONG g_cyDeadW = 0, g_cyDeadR = 0;
static void Cy_OnDeath(void *thisObj, void *params)
{
    (void)thisObj; void **p = (void **)params; void *pl = GetPlayer();
    if (p && p[0] && p[0] != pl && g_cyDeadW - g_cyDeadR < 32) { g_cyDead[g_cyDeadW & 31] = p[0]; InterlockedIncrement(&g_cyDeadW); }
    if (p && pl && p[1] == pl && p[0] && p[0] != pl) { InterlockedIncrement(&g_cyKills); InterlockedIncrement(&g_cyKillsL); }
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
    if (c->special == CYS_HACK) { Cy_HackOpen(); return; }
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
    if (g_cyBrainOk) dur += 4000.0;   /* (Cyber Brained: the ability lasts 4 s longer) */
    if (g_cyBrainOk) { double c0 = cool; cool -= 4000.0 * g_cyN; if (cool < 6000.0) cool = 6000.0;   /* (Cyber Brained: 4 s less per piece of chrome, 6 s at least) */
        Log("Cyber Brained: %d pieces of chrome - cooldown %.0f s instead of %.0f s", g_cyN, cool / 1000.0, c0 / 1000.0); }
    g_cyAbilEnd = now + dur; g_cyAbilReady = now + dur + cool;
    Cy_Buzz(3999);
    char m[96]; snprintf(m, sizeof m, "%s engaged", c->name); UIMessage(m);
    Log("Cyberware: %s engaged", c->name);
}
/* the two-squeeze gesture: both grips squeezed hard, let go, squeezed again - within 0.6 s */
static void Cy_Gesture(const VRControllerState_t *l, const VRControllerState_t *r, bool okL, bool okR)
{
    static int phase = 0; static double at = 0;
    if (!g_cyOn || !okL || !okR || !g_inGameplay || IsMenuMode() || g_pipOpen) { phase = 0; g_cyHackOpen = false; return; }
    {   /* jump (A) pressed: a second jump in the air with Kinetic Calves */
        static bool aPrev = false; bool a = Pressed(r, EVRButtonId_k_EButton_A);
        if (a && !aPrev && !g_cyDashGrip) g_cyJumpEdge = true;   /* (right grip + jump is the dash) */
        aPrev = a;
    }
    {   /* Cyber Eyes: left grip + left trigger held - scanning; the right stick (left/right) picks a quickhack, the right trigger uploads it */
        static bool centred = false, rtWas = false;
        bool scan = g_cyEyes && l->rAxis[2].x > 0.7f && l->rAxis[1].x > 0.6f;
        g_cyHackOpen = scan;
        float x = r->rAxis[0].x; bool rt = r->rAxis[1].x > 0.7f;
        if (scan) {
            if (fabsf(x) < 0.3f) centred = true;
            else if (centred && fabsf(x) > 0.7f) { centred = false; g_cyHackStep = x > 0 ? 1 : -1; }
            if (rt && !rtWas) g_cyHackFire = true;
        } else centred = false;
        rtWas = rt;
    }
    float gl = l->rAxis[2].x, gr = r->rAxis[2].x; double now = QpcMs();
    bool both = gl > 0.85f && gr > 0.85f, none = gl < 0.35f && gr < 0.35f;
    if (phase && now - at > 600.0) phase = 0;
    if (phase == 0 && both) { phase = 1; at = now; }
    else if (phase == 1 && none) phase = 2;
    else if (phase == 2 && both) { phase = 0; if (Cy_Core()) g_cyActivate = true; }
}
#include "fnvr_cyber_enemy.c"
/* ---- Cyber Eyes and Brain Dancer decks: scanning and quickhacks ----
   Cyber Eyes: hold the left grip and left trigger - everyone around is outlined (the game's Detect Life glow), and whoever
   your gun hand points at shows their health. With a Brain Dancer Deck too: the right stick (left / right) picks a
   quickhack and the right trigger uploads it to them (5 s each): Reboot Optics 3 RAM (they can't see you), Weapon Glitch 3
   (their weapon keeps failing), Cyberpsychosis 5 (frenzied: they attack whoever is nearest), Cyberware Malfunction 4 (their
   chrome shuts off), Rogue AI (all your RAM, only when full: a person or a robot dies outright). RAM refills 1 every 2 s
   (Mk.5: 5 a second). */
#define CY_HACK_MS 5000.0
static float g_cyRam = -1; static int g_cyHackSel = 0; static void *g_cyStunScr = NULL;
static struct { char *ref; uint32_t id; int kind, aggr; double until, next; } g_cyHk[16]; static int g_cyHkN = 0;
static int Cy_DeckTier(void) { const CyImpl *c = Cy_Core(); return c && c->special == CYS_HACK ? c->id[9] - '0' : 0; }
static int Cy_DeckRam(int t) { static const int r[6] = { 0, 5, 7, 10, 15, 20 }; return t >= 1 && t <= 5 ? r[t] : 0; }
static void Cy_HackOpen(void) { UIMessage(g_cyEyes ? "Quickhacks: hold the left grip + left trigger to scan, right trigger uploads" : "Quickhacks need Cyber Eyes (optics)"); }
static char *Cy_HackTarget(void)   /* the living character nearest the line your gun hand points along (within 8 degrees, 45 m) */
{
    int h = g_leftHanded ? 0 : 1; if (!g_handWOk[h]) return NULL;
    float f[3] = { -g_handWR[h][0][2], -g_handWR[h][1][2], -g_handWR[h][2][2] };
    char *best = NULL, *pl = (char *)GetPlayer(), *r = NULL; float bestA = cosf(8.0f * 3.14159265f / 180.0f); int guard = 0;
    while ((r = Ce_NextFoe(r)) != NULL && guard++ < 120) {
        if (r == pl || IsBadReadPtr(r, 0x40)) continue;
        const float *P = (const float *)(r + 0x30);
        float d[3] = { P[0] - g_handW[h][0], P[1] - g_handW[h][1], P[2] + 0.9f * g_unitsPerMeter - g_handW[h][2] };
        float L = V3Len(d); if (L < 1.0f || L > 45.0f * g_unitsPerMeter) continue;
        float c = (d[0] * f[0] + d[1] * f[1] + d[2] * f[2]) / L;
        if (c > bestA) { bestA = c; best = r; }
    }
    return best;
}
/* Rogue AI landing: a red shock runs over them, they lock up for a second - then it takes all their health (the kill is yours) */
static void Cy_RogueStart(char *ref)
{
    static void *sFx = NULL;
    if (!g_cyStunScr) g_cyStunScr = Cy_Compile("ref rT\nint iOn\nBegin Function { rT, iOn }\nrT.SetRestrained iOn\nEnd\n");
    if (!sFx) sFx = Cy_Compile("ref rT\nBegin Function { rT }\nrT.PlayMagicShaderVisuals LaserCritGlowFXShader 1.2\nrT.PlayMagicShaderVisuals DLC03TeslaFXShader 1.2\nEnd\n");
    uint32_t id = *(uint32_t *)(ref + 0x0C); double now = QpcMs();
    int i; for (i = 0; i < g_cyHkN; i++) if (g_cyHk[i].id == id && g_cyHk[i].kind == 6) return;   /* (already on its way) */
    if (g_cyHkN >= 16) { g_cyHkN--; }   /* (the list full: the oldest goes - this one matters more) */
    i = g_cyHkN++; g_cyHk[i].ref = ref; g_cyHk[i].id = id; g_cyHk[i].kind = 6; g_cyHk[i].until = now + 1000.0; g_cyHk[i].next = g_cyHk[i].until;
    Ce_Call(sFx, ref, 1, 0, NULL);
    Ce_Call(g_cyStunScr, ref, 2, 1, NULL);
}
static void Cy_Note(const char *m) { if (g_gameOk) QueueUIMessage(m, 0, NULL, NULL, 1.0f, false); }   /* (a short one: the scan overlay keeps them coming) */
static void Cy_HackTick(void)
{
    double now = QpcMs(); static double last = 0;
    g_cyEyes = g_cyOn && Cy_Has(CYS_EYES);
    int t = Cy_DeckTier(), cap = Cy_DeckRam(t);
    if (!t) g_cyRam = -1;
    else {   /* RAM refills */
        if (g_cyRam < 0) g_cyRam = (float)cap;
        float dt = last > 0 ? (float)((now - last) / 1000.0) : 0; if (dt > 0.5f) dt = 0.5f;
        g_cyRam += dt * (t == 5 ? 5.0f : 0.5f); if (g_cyRam > cap) g_cyRam = (float)cap;
    }
    last = now;
    static void *sBlind, *sSeeAgain, *sStop, *sDisarm, *sKill, *sKind, *sNearest, *sGlow, *sGlowOff; static bool tried = false;
    if (!tried && (t || g_cyEyes || g_cyHkN)) { tried = true;
        sBlind = Cy_Compile("ref rT\nBegin Function { rT }\nrT.ModAV Perception -10\nrT.StopCombat\nEnd\n");
        sSeeAgain = Cy_Compile("ref rT\nBegin Function { rT }\nrT.ModAV Perception 10\nEnd\n");
        sStop = Cy_Compile("ref rT\nBegin Function { rT }\nif rT.IsInCombat\nrT.StopCombat\nendif\nEnd\n");
        sDisarm = Cy_Compile("ref rT\nref rW\nBegin Function { rT }\nset rW to rT.GetEquippedObject 5\nif rW\nrT.UnequipItem rW 0 1\nendif\nEnd\n");
        sKill = Cy_Compile("ref rT\nBegin Function { rT }\nrT.Kill player\nEnd\n");
        sKind = Cy_Compile("ref rT\nint iV\nBegin Function { rT }\nset iV to rT.GetCreatureType\nSetFunctionValue iV\nEnd\n");
        sNearest = Cy_Compile("ref rW\nref rT\nref rBest\nfloat fD\nfloat fBest\nBegin Function { rW }\nset fBest to 3000\nset rT to GetFirstRef 200 1\nwhile rT != 0\nif rT != rW\nif rT != player\nif rT.GetDead == 0\nset fD to rW.GetDistance rT\nif fD < fBest\nset fBest to fD\nset rBest to rT\nendif\nendif\nendif\nendif\nset rT to GetNextRef\nloop\nif rBest\nrW.StartCombat rBest\nendif\nEnd\n");
        /* (the scan: everyone alive within 45 m glows for a moment - renewed while you hold it) */
        sGlow = Cy_Compile("ref rT\nBegin Function { }\nset rT to GetFirstRef 200 1\nwhile rT != 0\nif rT != player\nif rT.GetDead == 0\nif player.GetDistance rT < 3150\nrT.PlayMagicShaderVisuals LifeDetected 1.5\nendif\nendif\nendif\nset rT to GetNextRef\nloop\nEnd\n");
        sGlowOff = Cy_Compile("ref rT\nBegin Function { }\nset rT to GetFirstRef 200 1\nwhile rT != 0\nif rT != player\nrT.StopMagicShaderVisuals LifeDetected\nendif\nset rT to GetNextRef\nloop\nEnd\n");
        Log("Cyber Eyes / Brain Dancer: scan %s, quickhacks %s", sGlow ? "ready" : "FAILED", sBlind && sDisarm && sKill && sNearest ? "ready" : "partly FAILED");
    }
    /* the hacks running on people */
    for (int i = 0; i < g_cyHkN; ) {
        char *r = g_cyHk[i].ref; bool live = LookupForm(g_cyHk[i].id) == (void *)r && Ce_Num(2, r) < 0.5;
        if (!live || now >= g_cyHk[i].until) {
            if (live && g_cyHk[i].kind == 6) { Ce_Call(g_cyStunScr, r, 2, 0, NULL); Ce_Call(sKill, r, 1, 0, NULL); Log("Rogue AI: %s (%08X) - all their health gone", Cy_RefName(r), g_cyHk[i].id); }
            if (live) { if (g_cyHk[i].kind == 5) Ce_Call(g_cyStunScr, r, 2, 0, NULL); if (g_cyHk[i].kind == 0) Ce_Call(sSeeAgain, r, 1, 0, NULL); if (g_cyHk[i].kind == 2) { Ce_SetAggr(r, g_cyHk[i].aggr); Ce_Call(sStop, r, 1, 0, NULL); } }
            g_cyHk[i] = g_cyHk[--g_cyHkN]; continue;
        }
        if (now >= g_cyHk[i].next) {
            if (g_cyHk[i].kind == 0) { Ce_Call(sStop, r, 1, 0, NULL); g_cyHk[i].next = now + 1000.0; }   /* (blind: they keep losing you) */
            else if (g_cyHk[i].kind == 1) { Ce_Call(sDisarm, r, 1, 0, NULL); g_cyHk[i].next = now + 500.0; }   /* (their weapon keeps glitching out) */
            else if (g_cyHk[i].kind == 2) { Ce_Call(sNearest, r, 1, 0, NULL); g_cyHk[i].next = now + 2000.0; }   /* (a new victim now and then) */
            else if (g_cyHk[i].kind == 5) { Ce_Call(g_cyStunScr, r, 2, 1, NULL); g_cyHk[i].next = g_cyHk[i].until; }   /* (Quickie: stunned) */
            else g_cyHk[i].next = g_cyHk[i].until;
        }
        i++;
    }
    /* scanning */
    static bool scanWas = false; static double glowAt = 0, infoAt = 0, tgtAt = 0; static char *tgt = NULL; static uint32_t tgtId = 0, shownId = 0; static int shownSel = -1;
    bool scan = g_cyHackOpen && g_cyEyes;
    if (!scan) {
        if (scanWas && sGlowOff && g_scriptIntf && g_scriptIntf->CallFunctionAlt) g_scriptIntf->CallFunctionAlt(sGlowOff, GetPlayer(), 0);
        scanWas = false; g_cyHackFire = false; g_cyHackStep = 0; tgt = NULL; shownId = 0; shownSel = -1; return;
    }
    if (!scanWas) { scanWas = true; glowAt = 0; Cy_Buzz(800); }
    if (now >= glowAt && sGlow && g_scriptIntf->CallFunctionAlt) { glowAt = now + 1000.0; g_scriptIntf->CallFunctionAlt(sGlow, GetPlayer(), 0); }
    if (now >= tgtAt) { tgtAt = now + 400.0; tgt = Cy_HackTarget(); tgtId = tgt ? *(uint32_t *)(tgt + 0x0C) : 0; }
    if (tgt && LookupForm(tgtId) != (void *)tgt) { tgt = NULL; tgtId = 0; }
    static const char *const nm[5] = { "Reboot Optics", "Weapon Glitch", "Cyberpsychosis", "Cyberware Malfunction", "Rogue AI" };
    int cost[5] = { 3, 3, 5, 4, cap };
    if (g_cyHackStep) { g_cyHackSel = (g_cyHackSel + g_cyHackStep + 5) % 5; g_cyHackStep = 0; }
    if (tgtId != shownId || g_cyHackSel != shownSel || now >= infoAt) {   /* the overlay text: who, their health, your RAM and the hack picked */
        infoAt = now + 1000.0; shownId = tgtId; shownSel = g_cyHackSel;
        char m[200], who[96] = "no target";
        if (tgt) snprintf(who, sizeof who, "%s  HP %.0f%%", Cy_RefName(tgt), Ce_Num(0, tgt) * 100.0);
        if (t) snprintf(m, sizeof m, "%s  |  RAM %d/%d  |  < %s (%d) >", who, (int)g_cyRam, cap, nm[g_cyHackSel], cost[g_cyHackSel]);
        else snprintf(m, sizeof m, "%s", who);
        Cy_Note(m);
    }
    if (!g_cyHackFire) return;
    g_cyHackFire = false;
    if (!t) return;   /* (Cyber Eyes alone: just the scan) */
    int k = g_cyHackSel;
    if (!tgt) { Cy_Note("Quickhack: no one there - point your gun hand at them"); Cy_Buzz(300); return; }
    if (k == 4 && g_cyRam < cap - 0.01f) { char m[96]; snprintf(m, sizeof m, "Rogue AI needs a full deck - RAM %d/%d", (int)g_cyRam, cap); Cy_Note(m); Cy_Buzz(300); return; }
    if (g_cyRam < cost[k] - 0.01f) { char m[96]; snprintf(m, sizeof m, "Not enough RAM for %s (%d) - %d/%d", nm[k], cost[k], (int)g_cyRam, cap); Cy_Note(m); Cy_Buzz(300); return; }
    char *r = tgt; uint32_t id = tgtId;
    bool ok = true;
    if (*(BYTE *)(r + 0x04) == 0x3C) {   /* (creatures: only robots can be hacked) */
        double kd = -1; Ce_Call(sKind, r, 1, 0, &kd);
        if ((int)kd != 6) { Cy_Note("Quickhacks only work on people and robots"); Cy_Buzz(300); return; }
    }
    if (k == 3) ok = Ce_ChromeOff(r, now + CY_HACK_MS);
    if (!ok) { Cy_Note("No cyberware in them to shut off"); Cy_Buzz(300); return; }
    if (k == 4) Cy_RogueStart(r);
    else if (k != 3) {   /* (one hack of each kind per person: a new one starts it over) */
        int i; for (i = 0; i < g_cyHkN; i++) if (g_cyHk[i].id == id && g_cyHk[i].kind == k) break;
        if (i == g_cyHkN) {
            if (g_cyHkN >= 16) { Cy_Note("Quickhack: too many hacks running"); return; }
            g_cyHk[i].ref = r; g_cyHk[i].id = id; g_cyHk[i].kind = k; g_cyHkN++;
            if (k == 0) Ce_Call(sBlind, r, 1, 0, NULL);
            if (k == 2) { int a = (int)Ce_Num(3, r); g_cyHk[i].aggr = a >= 0 && a <= 3 ? a : 1; Ce_SetAggr(r, 3); Ce_Call(sStop, r, 1, 0, NULL); }
        }
        g_cyHk[i].until = now + CY_HACK_MS; g_cyHk[i].next = now;
    }
    g_cyRam -= (float)cost[k];
    char m[128]; snprintf(m, sizeof m, "%s uploaded: %s - RAM %d/%d", nm[k], Cy_RefName(r), (int)g_cyRam, cap); UIMessage(m);
    Cy_Buzz(3999); shownId = 0;
    Log("Brain Dancer: %s on %s (%08X), %d RAM left of %d", nm[k], Cy_RefName(r), id, (int)g_cyRam, cap);
}
/* Quickie (legendary mantis blade): its 30% proc - Cyberware Malfunction for 5 s (chromed or not, the jolt lands), shocked, stunned 1 s */
static void Cy_QuickieProc(char *ref, uint32_t id)
{
    double now = QpcMs();
    if (!g_cyStunScr) g_cyStunScr = Cy_Compile("ref rT\nint iOn\nBegin Function { rT, iOn }\nrT.SetRestrained iOn\nEnd\n");
    bool chrome = Ce_ChromeOff(ref, now + CY_HACK_MS);
    int i; for (i = 0; i < g_cyHkN; i++) if (g_cyHk[i].id == id && g_cyHk[i].kind == 5) break;
    if (i == g_cyHkN && g_cyHkN < 16) { g_cyHk[i].ref = ref; g_cyHk[i].id = id; g_cyHk[i].kind = 5; g_cyHkN++; }
    if (i < 16) { g_cyHk[i].until = now + 1000.0; g_cyHk[i].next = now; }
    char m[96]; snprintf(m, sizeof m, "Quickie: %s %s", Cy_RefName(ref), chrome ? "- cyberware offline" : "jolted"); UIMessage(m);
    Log("Quickie: %08X - %s, shocked, stunned 1 s", id, chrome ? "Cyberware Malfunction" : "no chrome to shut off");
}
/* ---- arm weapons: skills, the effects they put on what they hit, the arm launcher ---- */
static int g_cySkExpl = 0, g_cySkMelee = 0, g_cySkUnarmed = 0, g_cyCrit = 0; static bool g_cyBarrageOk = false;
/* ---- Praying Mantis (perk: a mantis blade, Agility 10, Endurance 10) ----
   every blade hit adds a bleed stack (no limit) and sets the one it hit bleeding (10 s, renewed by each hit). While anyone is
   bleeding: each stack heals you 5% of your health a second (20 stacks: 100%/s), and every bleeding enemy loses 1% of their
   health a second per stack (20 stacks: 20%/s). When the last bleed ends (dead or run out) the stacks reset. */
static bool g_cyMantisOk = false;
static int g_cyMantisStacks = 0;
static struct { char *ref; uint32_t id; double until; } g_cyMantis[32]; static int g_cyMantisN = 0;
static void Cy_MantisHit(char *ref, uint32_t id)
{
    double now = QpcMs(); int i;
    for (i = 0; i < g_cyMantisN; i++) if (g_cyMantis[i].id == id && g_cyMantis[i].ref == ref) break;
    if (i == g_cyMantisN) { if (g_cyMantisN >= 32) return; g_cyMantis[g_cyMantisN].ref = ref; g_cyMantis[g_cyMantisN].id = id; g_cyMantisN++; }
    g_cyMantis[i].until = now + 10000.0;
    g_cyMantisStacks++;
    if (g_cyMantisStacks == 1 || g_cyMantisStacks % 5 == 0) { char m[80]; snprintf(m, sizeof m, "Praying Mantis x%d", g_cyMantisStacks); UIMessage(m); }
    static int l = 0; if (l++ < 40) Log("Praying Mantis: %08X bleeding - %d stacks", id, g_cyMantisStacks);
}
static void Cy_MantisTick(void)
{
    static double next = 0; double now = QpcMs();
    if (!g_cyMantisN) { if (g_cyMantisStacks) { Log("Praying Mantis: every bleed is over - %d stacks reset", g_cyMantisStacks); g_cyMantisStacks = 0; } return; }
    if (now < next) return; next = now + 250.0;   /* (four ticks a second, a quarter of the per-second amounts each) */
    static void *dmg = NULL, *heal = NULL; static bool bad = false;
    if (!dmg && !bad) {
        dmg = Cy_Compile("ref rWho\nint iP\nfloat fM\nfloat fD\nfloat fHP\nBegin Function { rWho, iP }\nset fHP to rWho.GetAV Health\nif fHP > 0\n"
                         "set fM to rWho.GetBaseAV Health\nset fD to fM * iP / 10000\nif fD >= fHP\nrWho.Kill player\nset fHP to 0\nelse\nrWho.DamageAV Health fD\nset fHP to fHP - fD\nendif\nendif\nSetFunctionValue fHP\nEnd\n");
        heal = Cy_Compile("int iP\nfloat fM\nBegin Function { iP }\nset fM to player.GetBaseAV Health\nset fM to fM * iP / 10000\nplayer.RestoreAV Health fM\nEnd\n");
        bad = !dmg || !heal; if (bad) Log("Praying Mantis: couldn't set up its scripts");
    }
    if (bad) { g_cyMantisN = 0; return; }
    int pDmg = g_cyMantisStacks * 100 / 4, pHeal = g_cyMantisStacks * 500 / 4;   /* (hundredths of a percent per tick) */
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    CallFn_t call = (CallFn_t)g_scriptIntf->CallFunction; if (!call || !GetPlayer()) return;
    for (int i = 0; i < g_cyMantisN; ) {
        bool live = now < g_cyMantis[i].until && LookupForm(g_cyMantis[i].id) == (void *)g_cyMantis[i].ref;
        if (live) {
            struct { double num; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
            live = call(dmg, GetPlayer(), NULL, &res, 2, g_cyMantis[i].ref, pDmg) && res.type == 1 && res.num > 0;
        }
        if (!live) { g_cyMantis[i] = g_cyMantis[--g_cyMantisN]; continue; }
        i++;
    }
    if (g_cyMantisN) { struct { double num; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res); call(heal, GetPlayer(), NULL, &res, 1, pHeal); }
}
static void Cy_SkillsTick(void)
{
    static double at = 0; if (QpcMs() < at) return; at = QpcMs() + 2000.0;
    static void *scr = NULL; static bool bad = false;
    if (!scr && !bad) { scr = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetAV Explosives * 1000000 + player.GetAV MeleeWeapons * 1000 + player.GetAV Unarmed\nSetFunctionValue iV\nEnd\n"); bad = !scr; }
    double v = 0; if (!scr || !Cy_CallNum(scr, 0, 0, &v) || v < 0) return;
    long long n = (long long)(v + 0.5); g_cySkExpl = (int)(n / 1000000); g_cySkMelee = (int)(n / 1000 % 1000); g_cySkUnarmed = (int)(n % 1000);
    static void *se = NULL; static bool sbad = false;   /* (Strength, Agility, Endurance: Cyber Gorilla needs STR 10 + END 10, Praying Mantis AGI 10 + END 10) */
    if (!se && !sbad) { se = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetAV Strength * 10000 + player.GetAV Agility * 100 + player.GetAV Endurance\nSetFunctionValue iV\nEnd\n"); sbad = !se; }
    static int en = 0; double w = 0; if (se && Cy_CallNum(se, 0, 0, &w) && w >= 0) {
        int n2 = (int)(w + 0.5), st = n2 / 10000, ag = n2 / 100 % 100; en = n2 % 100;
        bool ok = st >= 10 && en >= 10;
        if (ok != g_cyGorillaOk && g_cyMechArms) UIMessage(ok ? "Cyber Gorilla: active" : "Cyber Gorilla needs Strength 10 and Endurance 10");
        g_cyGorillaOk = ok;
        bool blades = (g_cyArmKindH[0] == 1 && g_cyArmDmg[0] > 0) || (g_cyArmKindH[1] == 1 && g_cyArmDmg[1] > 0);
        bool mk = ag >= 10 && en >= 10;
        if (mk != g_cyMantisOk && blades) UIMessage(mk ? "Praying Mantis: active" : "Praying Mantis needs Agility 10 and Endurance 10");
        g_cyMantisOk = mk; }
    static void *sl = NULL; static bool lbad = false;   /* (Luck and crit chance: Missile Barrage) */
    if (!sl && !lbad) { sl = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetAV Luck * 1000 + player.GetAV CritChance\nSetFunctionValue iV\nEnd\n"); lbad = !sl; }
    double u = 0; if (sl && Cy_CallNum(sl, 0, 0, &u) && u >= 0) {
        int n3 = (int)(u + 0.5); g_cyCrit = n3 % 1000;
        bool bo = n3 / 1000 >= 10 && en >= 10;
        bool launcher = g_cyLaunchH[0] >= 0 || g_cyLaunchH[1] >= 0;
        if (bo != g_cyBarrageOk && launcher) UIMessage(bo ? "Missile Barrage: active - 5 rockets" : "Missile Barrage needs Luck 10 and Endurance 10");
        g_cyBarrageOk = bo; }
    static void *si = NULL; static bool ibad = false;   /* (Intelligence: Cyber Brained) */
    if (!si && !ibad) { si = Cy_Compile("int iV\nBegin Function { }\nset iV to player.GetAV Intelligence\nSetFunctionValue iV\nEnd\n"); ibad = !si; }
    double q = 0; if (si && Cy_CallNum(si, 0, 0, &q)) {
        bool br = q >= 9.5 && en >= 10;
        if (br != g_cyBrainOk && g_cyN > 0) UIMessage(br ? "Cyber Brained: active" : "Cyber Brained needs Intelligence 10 and Endurance 10");
        g_cyBrainOk = br; }
}
/* (from the hit code) the extra damage this hand's arm weapon adds to a blow: blades x(1 + Melee Weapons/100), Mech Arms
   x(1 + Unarmed/100) - double at 100 - and its effect queued to go on whoever was hit */
static float Cy_ArmBlow(int h, char *ref, uint32_t id)
{
    if (h < 0 || h > 1 || g_cyArmDmg[h] <= 0) return 0;
    float sk = g_cyArmKindH[h] == 2 ? (float)g_cySkUnarmed : (float)g_cySkMelee; if (sk > 100) sk = 100; if (sk < 0) sk = 0;
    if (g_cyArmKindH[h] == 1 && g_cyMantisOk && ref) Cy_MantisHit(ref, id);   /* (Praying Mantis: a blade hit is a bleed stack) */
    if (g_cyThunderH[h] && ref && rand() % 100 < 20 && g_cyFxN < 16) { g_cyFxQ[g_cyFxN].ref = ref; g_cyFxQ[g_cyFxN].id = id; g_cyFxQ[g_cyFxN].fx = 6; g_cyFxN++; }   /* (Thunder Cannon: discharge) */
    if (g_cyQuickieH[h] && ref && rand() % 100 < 30 && g_cyFxN < 16) { g_cyFxQ[g_cyFxN].ref = ref; g_cyFxQ[g_cyFxN].id = id; g_cyFxQ[g_cyFxN].fx = 5; g_cyFxN++; }   /* (Quickie: the daemon goes in) */
    if (g_cyArmFxH[h] && ref && g_cyFxN < 16) { g_cyFxQ[g_cyFxN].ref = ref; g_cyFxQ[g_cyFxN].id = id; g_cyFxQ[g_cyFxN].fx = g_cyArmFxH[h]; g_cyFxN++; }
    return g_cyArmDmg[h] * (1.0f + sk / 100.0f);
}
static void Cy_FxFlush(void)
{
    static void *scr = NULL; static bool bad = false;
    if (g_cyFxN && !scr && !bad) {
        scr = Cy_Compile("ref rWho\nint iK\nBegin Function { rWho, iK }\nif iK == 1\nrWho.CastImmediateOnSelf FNVVRMCEnchThermal\nelseif iK == 2\nrWho.CastImmediateOnSelf FNVVRMCEnchElectric\n"
                         "elseif iK == 3\nrWho.CastImmediateOnSelf FNVVRMCEnchToxic\nelseif iK == 4\nrWho.CastImmediateOnSelf FNVVRMCEnchBleed\nendif\nEnd\n");
        bad = !scr; if (bad) Log("Cyberware: arm weapon effects need FNVVRMC - Cyberware.esp");
    }
    for (int i = 0; i < g_cyFxN; i++)
        if (scr && LookupForm(g_cyFxQ[i].id) == (void *)g_cyFxQ[i].ref && g_scriptIntf->CallFunctionAlt) {
            if (g_cyFxQ[i].fx == 6) {   /* Thunder Cannon: 100 electric damage to them and every enemy within 6 m (not your companions) */
                static void *th = NULL; static bool thBad = false;
                if (!th && !thBad) { th = Cy_Compile("ref rT\nref rW\nfloat fH\nint iN\nBegin Function { rT }\nset iN to 0\nset rW to GetFirstRef 200 1\nwhile rW != 0\nif rW != player\nif rW.GetDead == 0\nif rW.GetDistance rT < 420\n"
                    "if rW == rT || rW.GetPlayerTeammate == 0\nset fH to rW.GetAV Health\nif fH <= 100\nrW.Kill player\nelse\nrW.DamageAV Health 100\nendif\nrW.CastImmediateOnSelf FNVVRMCEnchElectric\nrW.PlayMagicShaderVisuals PulseDisableFXShader 1\nset iN to iN + 1\nendif\nendif\nendif\nendif\nset rW to GetNextRef\nloop\nSetFunctionValue iN\nEnd\n");   /* (a while loop, like the other scripts here - a GoTo out of an if block can upset the game's if-tracking) */
                    thBad = !th; if (thBad) Log("Thunder Cannon: couldn't set up its discharge"); }
                double n = 0; if (th) Ce_Call(th, g_cyFxQ[i].ref, 1, 0, &n);
                char m[80]; snprintf(m, sizeof m, "THUNDER CANNON - %d hit", (int)n); UIMessage(m); Cy_Buzz(3999);
                Log("Thunder Cannon: discharge from %08X - %d hit for 100", g_cyFxQ[i].id, (int)n);
                continue;
            }
            if (g_cyFxQ[i].fx == 5) { Cy_QuickieProc(g_cyFxQ[i].ref, g_cyFxQ[i].id); g_scriptIntf->CallFunctionAlt(scr, GetPlayer(), 2, g_cyFxQ[i].ref, 2); continue; }
            g_scriptIntf->CallFunctionAlt(scr, GetPlayer(), 2, g_cyFxQ[i].ref, g_cyFxQ[i].fx);
            static int l = 0; static const char *const nm[] = { "", "burning", "shocked", "poisoned", "bleeding" };
            if (l++ < 20) Log("Cyberware: %08X %s (arm weapon)", g_cyFxQ[i].id, nm[g_cyFxQ[i].fx]);
        }
    g_cyFxN = 0;
}
/* the arm launcher: make a fist with that hand (grip) and pull the trigger - a mini-rocket out of the forearm. 3 rockets;
   empty, it refills itself in 20 s (5 s at Explosives 100, in between in step); 70 explosive damage x(1 + Explosives/100). */
static struct { int rounds; double refillAt; bool trigWas, gripWas, gripFirst; } g_cyLn[2] = { { 3, 0, false, false, false }, { 3, 0, false, false, false } };
static void *Cy_Form(void *scr)
{
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    if (!scr || !g_scriptIntf || !g_scriptIntf->CallFunction || !GetPlayer()) return NULL;
    struct { union { double num; void *form; } v; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    if (!((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 0) || res.type != 2) return NULL;
    return res.v.form;
}
/* Jack's Favorite's 0.05%: Rogue AI into whoever the beam is aimed at (within 4 degrees, 60 m) - a person or robot dies */
static void Cy_JackRogue(const float *o, const float *f)
{
    char *best = NULL, *pl = (char *)GetPlayer(), *r = NULL; float bestA = cosf(4.0f * 3.14159265f / 180.0f); int guard = 0;
    while ((r = Ce_NextFoe(r)) != NULL && guard++ < 120) {
        if (r == pl || IsBadReadPtr(r, 0x40)) continue;
        const float *P = (const float *)(r + 0x30);
        float d[3] = { P[0] - o[0], P[1] - o[1], P[2] + 0.9f * g_unitsPerMeter - o[2] };
        float L = V3Len(d); if (L < 1.0f || L > 60.0f * g_unitsPerMeter) continue;
        float c = (d[0] * f[0] + d[1] * f[1] + d[2] * f[2]) / L;
        if (c > bestA) { bestA = c; best = r; }
    }
    if (!best) return;
    static void *sK = NULL, *sKind = NULL;
    if (!sK) sK = Cy_Compile("ref rT\nBegin Function { rT }\nrT.Kill player\nEnd\n");
    if (!sKind) sKind = Cy_Compile("ref rT\nint iV\nBegin Function { rT }\nset iV to rT.GetCreatureType\nSetFunctionValue iV\nEnd\n");
    if (*(BYTE *)(best + 0x04) == 0x3C) { double kd = -1; Ce_Call(sKind, best, 1, 0, &kd); if ((int)kd != 6) return; }   /* (people and robots only) */
    (void)sK; Cy_RogueStart(best);
    char m[96]; snprintf(m, sizeof m, "Jack's Favorite: ROGUE AI uploaded - %s", Cy_RefName(best)); UIMessage(m); Cy_Buzz(3999);
    Log("Jack's Favorite: Rogue AI on %s (%08X)", Cy_RefName(best), *(uint32_t *)(best + 0x0C));
}
static void Cy_LaunchTick(void)
{
    static const char *const kn[6] = { "Frag", "Thermal", "Electric", "Toxic", "Shrapnel", "Jack" };
    static void *proj[6], *expl[6], *launcherW, *laserW; static int dmgOff[6]; static bool tried = false, ok = false;
    static struct { double fireLeft, reloadAt, nextShot, last; bool init; } jk[2];   /* (Jack's Favorite: seconds of beam left, recharge) */
    double now = QpcMs();
    float sk = (float)(g_cySkExpl > 100 ? 100 : g_cySkExpl < 0 ? 0 : g_cySkExpl);
    double refillMs = g_cyBrainOk ? 3000.0 : (20.0 - 15.0 * sk / 100.0) * 1000.0;   /* (Cyber Brained: 3 s) */
    int cap = g_cyBarrageOk ? 5 : 3;   /* (Missile Barrage: 5 rockets) */
    static bool capWas = false; static int lastH = -1; static double lastShot = 0;
    if (g_cyBarrageOk != capWas) { capWas = g_cyBarrageOk;   /* (perk on: a full launcher tops up to 5; off: down to 3) */
        for (int h = 0; h < 2; h++) { if (g_cyBarrageOk && g_cyLn[h].rounds == 3 && !g_cyLn[h].refillAt) g_cyLn[h].rounds = 5; if (g_cyLn[h].rounds > cap) g_cyLn[h].rounds = cap; } }
    {   /* Missile Barrage: a kill soon after a rocket - your crit chance to get 1 rocket back (over 50%: a 50% chance of 2) */
        LONG k = InterlockedExchange(&g_cyKillsL, 0);
        if (k && g_cyBarrageOk && lastH >= 0 && g_cyLaunchH[lastH] >= 0 && now - lastShot < 4000.0)
            for (LONG i = 0; i < k; i++) {
                int ch = g_cyCrit > 50 ? 50 : g_cyCrit, n = g_cyCrit > 50 ? 2 : 1;
                if (rand() % 100 >= ch) continue;
                int r0 = g_cyLn[lastH].rounds; g_cyLn[lastH].rounds = r0 + n > cap ? cap : r0 + n;
                if (g_cyLn[lastH].rounds >= cap) g_cyLn[lastH].refillAt = 0;
                char m[64]; snprintf(m, sizeof m, "Missile Barrage: +%d rocket%s", g_cyLn[lastH].rounds - r0, g_cyLn[lastH].rounds - r0 == 1 ? "" : "s"); if (g_cyLn[lastH].rounds > r0) UIMessage(m);
                Log("Missile Barrage: a rocket kill (crit %d%%) - %d -> %d rockets", g_cyCrit, r0, g_cyLn[lastH].rounds);
            }
    }
    if (g_cyLaunchH[0] >= 0 || g_cyLaunchH[1] >= 0) {
    if (!tried) {
        tried = true; char src[160]; ok = true;
        for (int k = 0; k < 6; k++) {
            snprintf(src, sizeof src, "ref r\nBegin Function { }\nset r to FNVVRMCArmRocket%s\nSetFunctionValue r\nEnd\n", kn[k]); void *s1 = Cy_Compile(src);
            snprintf(src, sizeof src, "ref r\nBegin Function { }\nset r to FNVVRMCArmRocketExplosion%s\nSetFunctionValue r\nEnd\n", kn[k]); void *s2 = Cy_Compile(src);
            proj[k] = Cy_Form(s1); expl[k] = Cy_Form(s2); dmgOff[k] = -1;
            if (expl[k]) for (int o = 0x18; o < 0x100; o += 4) if (!IsBadReadPtr((char *)expl[k] + o, 4) && *(float *)((char *)expl[k] + o) == 70.0f) { dmgOff[k] = o; break; }
            if ((!proj[k] || !expl[k]) && k < 5) ok = false;
            Log("Cyberware: arm launcher %s rocket %s, explosion damage %s", kn[k], proj[k] ? "found" : "MISSING", dmgOff[k] >= 0 ? "found" : "not found (fixed 70)");
        }
        launcherW = LookupForm(0x00004340);   /* (WeapMissileLauncher: the rockets count as an explosive weapon of yours) */
        laserW = LookupForm(0x0009073B);      /* (WeapNVLaserRCW: Jack's Favorite's bolts) */
        if (!ok) UIMessage("The arm launcher needs FNVVRMC - Cyberware.esp");
    }
    }
    for (int h = 0; h < 2; h++) {
        if (g_cyLaunchH[h] < 0 || (h == 0 && g_cyEyes)) { g_cyLn[h].trigWas = false; jk[h].init = false; continue; }   /* (Cyber Eyes: the left grip + trigger is the scan) */
        if (g_cyLn[h].refillAt > 0 && now >= g_cyLn[h].refillAt) {
            g_cyLn[h].rounds = cap; g_cyLn[h].refillAt = 0;
            if (g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 1500);
            Log("Cyberware: %s arm launcher refilled - %d rockets", h ? "right" : "left", cap);
        }
        if (g_gripHeld[h] && !g_cyLn[h].gripWas) g_cyLn[h].gripFirst = g_grapTrigH[h] < 0.35f;   /* (fist first, then trigger - a trigger pulled first stays bullet time) */
        if (!g_gripHeld[h]) g_cyLn[h].gripFirst = false;
        g_cyLn[h].gripWas = g_gripHeld[h];
        bool pull = g_gripHeld[h] && g_cyLn[h].gripFirst && g_grapTrigH[h] > 0.7f && g_handWOk[h] && !g_holding[h] && !g_pipOpen;
        int ah = g_leftHanded ? 0 : 1;
        if (h == ah && g_weaponOutThisFrame && PlayerWeaponType() > 2) pull = false;   /* (the gun hand with a gun out: its trigger fires the gun) */
        bool edge = pull && !g_cyLn[h].trigWas; g_cyLn[h].trigWas = pull;
        if (g_cyLaunchH[h] == 5) {   /* Jack's Favorite: a held beam - 20 bolts a second, 6 s of fire, then a 6 s recharge (Cyber Brained: 3 s) */
            if (!jk[h].init) { jk[h].init = true; jk[h].fireLeft = 6000.0; jk[h].reloadAt = 0; jk[h].last = now; }
            double dt = now - jk[h].last; jk[h].last = now; if (dt > 100.0) dt = 100.0;
            if (jk[h].reloadAt > 0 && now >= jk[h].reloadAt) { jk[h].reloadAt = 0; jk[h].fireLeft = 6000.0; if (g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 1500); Log("Jack's Favorite: recharged"); }
            if (!pull) { jk[h].nextShot = 0; continue; }
            if (jk[h].reloadAt > 0) { if (edge) { char m[80]; snprintf(m, sizeof m, "Jack's Favorite recharging (%.0f s)", (jk[h].reloadAt - now) / 1000.0); UIMessage(m); } continue; }
            if (!ok || !proj[5] || !expl[5] || !o_ProjSpawn || !laserW) { if (edge) UIMessage("Jack's Favorite needs the newest FNVVRMC - Cyberware.esp"); continue; }
            jk[h].fireLeft -= dt;
            if (jk[h].nextShot < now - 100.0) jk[h].nextShot = now;
            int shots = 0;
            while (now >= jk[h].nextShot && shots < 3) {
                jk[h].nextShot += 50.0; shots++;
                float d[3]; bool fd = now - g_foreDirAt[h] < 150.0 && V3Len(g_foreDirW[h]) > 0.5f;
                for (int i = 0; i < 3; i++) d[i] = fd ? g_foreDirW[h][i] : -g_handWR[h][i][2];
                float dl = V3Len(d); if (dl < 0.5f) break; for (int i = 0; i < 3; i++) d[i] /= dl;
                float pos[3]; for (int i = 0; i < 3; i++) pos[i] = g_handW[h][i] + d[i] * 12.0f + g_handWR[h][i][1] * 2.0f;
                char *player = (char *)GetPlayer(); void *cell = *(void **)(player + 0x40);
                float hh = atan2f(d[0], d[1]), pp = -asinf(fminf(fmaxf(d[2], -1.0f), 1.0f));
                void *r = o_ProjSpawn(proj[5], player, NULL, laserW, pos[0], pos[1], pos[2], hh, pp, NULL, NULL, 0, 0, 0.0f, 0.0f, (uint32_t)(uintptr_t)cell);
                if (r) { int fo[4]; int nfo = BT_DirOffsets((char *)r, hh, pp, fo); BT_Add((char *)r, hh, pp, fo, nfo); }
                if (rand() % 10000 < 5) Cy_JackRogue(pos, d);   /* (0.05%: Rogue AI) */
            }
            if (shots && g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 900);
            if (jk[h].fireLeft <= 0) {
                jk[h].reloadAt = now + (g_cyBrainOk ? 3000.0 : 6000.0); jk[h].nextShot = 0;
                UIMessage("Jack's Favorite overheated - recharging"); Log("Jack's Favorite: 6 s of fire spent - recharging");
            }
            continue;
        }
        if (!edge) continue;
        if (g_cyLn[h].rounds <= 0) {
            if (g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 300);
            char m[80]; snprintf(m, sizeof m, "Arm launcher refilling (%.0f s)", g_cyLn[h].refillAt > now ? (g_cyLn[h].refillAt - now) / 1000.0 : 0.0); UIMessage(m);
            continue;
        }
        int k = g_cyLaunchH[h];
        if (!ok || !proj[k] || !o_ProjSpawn) continue;
        float dmg = 70.0f * (1.0f + sk / 100.0f);
        if (dmgOff[k] >= 0) *(float *)((char *)expl[k] + dmgOff[k]) = dmg;
        float d[3]; bool fd = now - g_foreDirAt[h] < 150.0 && V3Len(g_foreDirW[h]) > 0.5f;
        for (int i = 0; i < 3; i++) d[i] = fd ? g_foreDirW[h][i] : -g_handWR[h][i][2];
        float dl = V3Len(d); if (dl < 0.5f) continue; for (int i = 0; i < 3; i++) d[i] /= dl;
        float pos[3]; for (int i = 0; i < 3; i++) pos[i] = g_handW[h][i] + d[i] * 12.0f + g_handWR[h][i][1] * 2.0f;
        char *player = (char *)GetPlayer(); void *cell = *(void **)(player + 0x40);
        float hh = atan2f(d[0], d[1]), pp = -asinf(fminf(fmaxf(d[2], -1.0f), 1.0f));
        void *r = o_ProjSpawn(proj[k], player, NULL, launcherW, pos[0], pos[1], pos[2], hh, pp, NULL, NULL, 0, 0, 0.0f, 0.0f, (uint32_t)(uintptr_t)cell);
        if (r) { int fo[4]; int nfo = BT_DirOffsets((char *)r, hh, pp, fo); BT_Add((char *)r, hh, pp, fo, nfo); }
        g_cyLn[h].rounds--; lastH = h; lastShot = now;
        if (g_cyLn[h].rounds <= 0 && !g_cyLn[h].refillAt) g_cyLn[h].refillAt = now + refillMs;
        if (g_vrSystem && g_handIdx[h] < FNVR_MAXDEV) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 3999);
        Log("Cyberware: %s arm launcher - %s rocket (%.0f damage), %d left%s", h ? "right" : "left", kn[k], dmg, g_cyLn[h].rounds,
            g_cyLn[h].rounds <= 0 ? " - refilling" : "");
    }
}
/* Dash Actuators: right grip held + jump - a quick burst about 5 feet (1.5 m) the way the left stick points, for 20 AP */
static void Cy_DashTick(void)
{
    static struct { bool on; float dir[3], left; double last; } d;
    char *player = (char *)GetPlayer(); if (!player) return;
    double now = QpcMs();
    if (g_cyDashReq) {
        g_cyDashReq = false;
        if (!Cy_Has(CYS_DASH) || d.on || g_climbing || Grapple_Busy()) goto run;
        static void *apScr = NULL; static bool bad = false;
        if (!apScr && !bad) { apScr = Cy_Compile("int iOk\nBegin Function { }\nset iOk to 0\nif player.GetAV ActionPoints >= 20\nplayer.DamageAV ActionPoints 20\nset iOk to 1\nendif\nSetFunctionValue iOk\nEnd\n"); bad = !apScr; }
        double ok = 0;
        if (!apScr || !Cy_CallNum(apScr, 0, 0, &ok) || ok < 0.5) { UIMessage("Not enough Action Points to dash (20)"); Cy_Buzz(300); goto run; }
        float hd = *PlayerRotZ(player), sx = g_cyDashStick[0], sy = g_cyDashStick[1];
        if (sx * sx + sy * sy < 0.04f) { sx = 0; sy = 1; }   /* (stick at rest: straight ahead) */
        float fw[2] = { sinf(hd), cosf(hd) }, rt[2] = { cosf(hd), -sinf(hd) };
        d.dir[0] = fw[0] * sy + rt[0] * sx; d.dir[1] = fw[1] * sy + rt[1] * sx; d.dir[2] = 0;
        float l = sqrtf(d.dir[0] * d.dir[0] + d.dir[1] * d.dir[1]); d.dir[0] /= l; d.dir[1] /= l;
        d.left = 1.524f * g_unitsPerMeter; d.on = true; d.last = now;
        Cy_Buzz(3999);
        static int ln = 0; if (ln++ < 10) Log("Cyberware: dash (stick %.2f %.2f)", sx, sy);
    }
run:
    if (!d.on) return;
    float dt = (float)((now - d.last) / 1000.0); d.last = now; if (dt > 0.05f) dt = 0.05f;
    float step = fminf(d.left, 11.0f * g_unitsPerMeter * dt);   /* (11 m/s: the 5 feet in about a seventh of a second) */
    if (step <= 0.01f || g_climbing || Grapple_Busy() || !g_inGameplay) { d.on = false; return; }
    float *P = (float *)(player + 0x30);
    for (int k = 0; k < 2; k++) {   /* a wall in the way (knee and chest height) ends it there */
        float a[3] = { P[0], P[1], P[2] + (k ? 0.45f : 1.2f) * g_unitsPerMeter }, b[3] = { a[0] + d.dir[0] * (step + 25.0f), a[1] + d.dir[1] * (step + 25.0f), a[2] }, fr;
        char *o = RayCast(a, b, &fr);
        if (o && fr < 1.0f) { d.on = false; return; }
    }
    float np[3] = { P[0] + d.dir[0] * step, P[1] + d.dir[1] * step, P[2] };
    Climb_MovePlayer(player, np);
    d.left -= step; if (d.left <= 0.01f) d.on = false;
}
static bool g_cyTalkHunter = false;
static bool g_cyFromDoc;
/* does any text in this piece of the interface (or below it) contain needle */
static bool Cy_TileHasText(char *t, const char *needle, int depth, int *budget)
{
    if (!Rd(t, 0x30) || depth > 16 || --(*budget) < 0) return false;
    char **vals = *(char ***)(t + 0x14); uint32_t nv = *(uint32_t *)(t + 0x18);
    if (vals && nv < 4096 && Rd(vals, nv * 4))
        for (uint32_t i = 0; i < nv; i++) {
            char *v = vals[i];
            if (!Rd(v, 0x14) || *(uint32_t *)v != 0xFC4 /* string */) continue;
            const char *s = *(const char **)(v + 0x0C);
            if (Rd(s, 1) && !IsBadStringPtrA(s, 1024) && strstr(s, needle)) return true;
        }
    char *node = *(char **)(t + 0x04);
    for (int k = 0; node && k < 300 && Rd(node, 12); k++, node = *(char **)node)
        if (Cy_TileHasText(*(char **)(node + 8), needle, depth + 1, budget)) return true;
    return false;
}
/* (diagnostics) the texts on the dialogue screen while talking to Hunter, logged once per line he says */
static void Cy_TileTexts(char *t, int depth, int *budget, char *out, size_t cap)
{
    if (!Rd(t, 0x30) || depth > 16 || --(*budget) < 0) return;
    char **vals = *(char ***)(t + 0x14); uint32_t nv = *(uint32_t *)(t + 0x18);
    if (vals && nv < 4096 && Rd(vals, nv * 4))
        for (uint32_t i = 0; i < nv; i++) {
            char *v = vals[i];
            if (!Rd(v, 0x14) || *(uint32_t *)v != 0xFC4) continue;
            const char *s = *(const char **)(v + 0x0C);
            if (!Rd(s, 1) || IsBadStringPtrA(s, 1024) || strlen(s) < 12) continue;
            size_t l = strlen(out); if (l + 70 < cap) snprintf(out + l, cap - l, "%s\"%.60s\"", l ? ", " : "", s);
        }
    char *node = *(char **)(t + 0x04);
    for (int k = 0; node && k < 300 && Rd(node, 12); k++, node = *(char **)node) Cy_TileTexts(*(char **)(node + 8), depth + 1, budget, out, cap);
}
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
            if (nm || !tries) g_cyTalkHunter = nm && !strcmp(nm, "Hunter");
        }
        if (!dlg) g_cyTalkHunter = false;
        /* Hunter's answer to "What kind of chrome do you have?" - "Everything you need." - on screen: the ripperdoc opens
           (2.13.2: read from the dialogue itself, so it works however the topic was picked) */
        static bool hSaid = false; static int hScan = 0;
        if (g_cyTalkHunter && ++hScan >= 6) {
            hScan = 0;
            char *dm = Vc_DialogTileMenu(); int budget = 4000;
            {   static char lastT[600]; char now[600] = ""; int b2 = 4000; static int logs = 0;
                if (dm) Cy_TileTexts(dm, 0, &b2, now, sizeof now);
                if (strcmp(now, lastT) && logs < 60) { logs++; Log("Cyberware: Hunter's dialogue screen shows: %s", now[0] ? now : "(no text found)"); snprintf(lastT, sizeof lastT, "%s", now); } }
            bool said = dm && Cy_TileHasText(dm, "Everything you need", 0, &budget);
            if (said && !hSaid && !(g_setOpen && g_setPage == SP_CYBER)) {
                g_cyAtDoc = true;
                Log("Cyberware: Hunter - \"Everything you need.\" - opening the ripperdoc");
                Cyber_Open(true);
            }
            hSaid = said;
            /* 50 Legion ears clear your name with the NCR: back to Liked (infamy gone, fame up to 40 of the NCR's 80) */
            static bool cSaid = false; budget = 4000;
            bool clr = dm && Cy_TileHasText(dm, "record with the NCR cleaned up", 0, &budget);
            if (clr && !cSaid) {
                static void *cScr = NULL; static bool cBad = false;
                if (!cScr && !cBad) {
                    cScr = Cy_Compile("float fF\nfloat fI\nint iN\nBegin Function { }\nset iN to player.GetItemCount LegionEar\nif iN >= 50\n"
                                      "player.RemoveItem LegionEar 50\nset fI to GetReputation RepNVNCR 0\nif fI > 0\nRemoveReputationExact RepNVNCR 0 fI\nendif\n"
                                      "set fF to GetReputation RepNVNCR 1\nif fF < 40\nset fF to 40 - fF\nAddReputationExact RepNVNCR 1 fF\nendif\nSetFunctionValue 1\nendif\nEnd\n");
                    cBad = !cScr; if (cBad) Log("Cyberware: the NCR name-clearing script wasn't accepted");
                }
                double ok = 0;
                if (cScr && Cy_CallNum(cScr, 0, 0, &ok) && ok > 0.5) Log("Cyberware: Hunter took 50 Legion ears - NCR reputation back to Liked");
            }
            cSaid = clr;
            /* his scripture talk ("I want you to have this"): the footlocker key, once; his Caesar talk ("some new chrome, if you
               want to take a look"): the footlocker becomes yours - taking from it is no longer stealing */
            static const struct { const char *said; const char *src; const char *log; } hx[2] = {
                { "I want you to have this", "int iN\nBegin Function { }\nset iN to player.GetItemCount FNVVRMCHunterFootlockerKey\nif iN == 0\nplayer.AddItem FNVVRMCHunterFootlockerKey 1\nSetFunctionValue 1\nendif\nEnd\n", "gave you his footlocker key" },
                { "if you want to take a look", "Begin Function { }\nFNVVRMCHunterFootlockerREF.SetOwnership\nSetFunctionValue 1\nEnd\n", "his footlocker is yours now (no more stealing)" } };
            static bool hxSaid[2]; static void *hxScr[2]; static bool hxBad[2];
            for (int k = 0; k < 2; k++) {
                budget = 4000; bool on = dm && Cy_TileHasText(dm, hx[k].said, 0, &budget);
                if (on && !hxSaid[k]) {
                    if (!hxScr[k] && !hxBad[k]) { hxScr[k] = Cy_Compile(hx[k].src); hxBad[k] = !hxScr[k]; }
                    double r = 0;
                    if (hxScr[k] && Cy_CallNum(hxScr[k], 0, 0, &r) && r > 0.5) Log("Cyberware: Hunter %s", hx[k].log);
                }
                hxSaid[k] = on;
            }
            /* Fiend ears: "50 caps a Fiend ear" */
            static bool fSaid = false; budget = 4000;
            bool fe = dm && Cy_TileHasText(dm, "caps a Fiend ear", 0, &budget);
            if (fe && !fSaid) {
                static void *fScr = NULL; static bool fBad = false;
                if (!fScr && !fBad) {
                    fScr = Cy_Compile("int iN\nint iC\nBegin Function { }\nset iN to player.GetItemCount FNVVRMCFiendEar\nif iN > 0\n"
                                      "set iC to iN * 50\nplayer.RemoveItem FNVVRMCFiendEar iN\nplayer.AddItem Caps001 iC\nendif\nSetFunctionValue iN\nEnd\n");
                    fBad = !fScr;
                }
                double n = 0;
                if (fScr && Cy_CallNum(fScr, 0, 0, &n)) Log("Cyberware: Hunter bought %d Fiend ears (%d caps)", (int)n, (int)n * 50);
            }
            fSaid = fe;
            /* his Legion ear bounty: "<price> caps an ear" on screen (400 idolized by the NCR, 250 liked, 100 otherwise) -
               every Legion ear you carry is swapped for that many caps */
            static int eSaid = 0;
            int price = 0;
            static const int prices[] = { 400, 250, 100 };
            for (int pi = 0; pi < 3 && !price && dm; pi++) {
                char needle[32]; snprintf(needle, sizeof needle, "%d caps an ear", prices[pi]); budget = 4000;
                if (Cy_TileHasText(dm, needle, 0, &budget)) price = prices[pi];
            }
            bool ears = price != 0;
            if (price && price != eSaid) {
                static void *eScr = NULL; static bool eBad = false;
                if (!eScr && !eBad) {
                    eScr = Cy_Compile("int iP\nint iN\nint iC\nBegin Function { iP }\nset iN to player.GetItemCount LegionEar\nif iN > 0\n"
                                      "set iC to iN * iP\nplayer.RemoveItem LegionEar iN\nplayer.AddItem Caps001 iC\nendif\nSetFunctionValue iN\nEnd\n");
                    eBad = !eScr;
                }
                double n = 0;
                if (eScr && Cy_CallNum(eScr, 1, price, &n)) Log("Cyberware: Hunter bought %d Legion ears at %d caps each (%d caps)", (int)n, price, (int)n * price);
            }
            eSaid = price; (void)ears;
        }
        if (!g_cyTalkHunter) hSaid = false;
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
            if (hScr && Cy_CallNum(hScr, 0, 0, &v) && v > 0.5 && !(g_setOpen && g_setPage == SP_CYBER)) {
                g_cyAtDoc = true;
                Log("Cyberware: Hunter - opening the ripperdoc");
                Cyber_Open(true);
            }
        }
        if (!dlg && !(g_setOpen && g_cyFromDoc)) g_cyAtDoc = false;   /* (still at the ripperdoc while his screen stays open, even if the talk closed) */
        dlgWas = dlg;
    }
    if (!g_inGameplay || !GetPlayer()) return;
    {   float b[2] = { 0, 0 }; bool m[2] = { false, false }; int fx[2] = { 0, 0 }, ln[2] = { -1, -1 }, kd[2] = { 0, 0 }; bool gr[2] = { false, false }, qk[2] = { false, false }, th[2] = { false, false };
        if (g_cyOn) for (int i = 0; i < g_cyN; i++) { const char *id = g_cyInst[i].id; int sd = Cy_ArmSide(id); if (sd < 0) continue;
            bool mech = !strncmp(id, "arm_mech", 8), launch = !strncmp(id, "arm_launch", 10);
            if (!strncmp(id, "arm_quickie", 11)) for (int h = 0; h < 2; h++) if (sd == h) qk[h] = true;
            if (!strncmp(id, "arm_mechthunder", 15)) for (int h = 0; h < 2; h++) if (sd == h) th[h] = true;
            if (!strncmp(id, "arm_grapple", 11)) { for (int h = 0; h < 2; h++) if (sd == h || sd == 2) gr[h] = true; continue; }
            int e = Cy_ArmFx(id);
            for (int h = 0; h < 2; h++) if (sd == h || sd == 2) {
                if (mech) m[h] = true;
                if (launch) ln[h] = Cy_LaunchKind(id);
                else { if (sd != 2) b[h] += Cy_ArmBonus(id); kd[h] = mech ? 2 : 1; fx[h] = e; } } }   /* (the old pairs raised Unarmed Damage itself) */
        for (int h = 0; h < 2; h++) { g_cyMechHand[h] = m[h]; g_cyArmDmg[h] = b[h]; g_cyArmKindH[h] = kd[h]; g_cyArmFxH[h] = fx[h]; g_cyLaunchH[h] = ln[h]; g_cyGrapH[h] = gr[h]; g_cyQuickieH[h] = qk[h]; g_cyThunderH[h] = th[h]; }
        g_cyMechArms = m[0] || m[1]; }
    Cy_SkillsTick();
    Cy_FxFlush();
    Cy_LaunchTick();
    Cy_MantisTick();
    Cy_HackTick();
    {   /* Blood Driver: 3% of your health back for every melee / unarmed hit that landed */
        int n = g_cyBloodHits; g_cyBloodHits = 0;
        if (n && Cy_Has(CYS_BLOOD)) {
            static void *scr = NULL; static bool bad = false;
            if (!scr && !bad) { scr = Cy_Compile("int iP\nfloat fM\nBegin Function { iP }\nset fM to player.GetBaseAV Health\nset fM to fM * iP / 100\nplayer.RestoreAV Health fM\nEnd\n"); bad = !scr; }
            double o = 0; if (scr) Cy_CallNum(scr, 1, 3 * n, &o);
            static int l = 0; if (l++ < 20) Log("Blood Driver: %d hit%s - %d%% health back", n, n == 1 ? "" : "s", 3 * n);
        }
    }
    while (g_cyDeadR != g_cyDeadW) {   /* the dead: a Fiend leaves an ear */
        void *who = g_cyDead[g_cyDeadR & 31]; InterlockedIncrement(&g_cyDeadR);
        static void *dScr = NULL; static bool dBad = false;
        if (!dScr && !dBad) {
            dScr = Cy_Compile("ref rV\nBegin Function { rV }\nif rV.GetInFaction FiendsFactionNV\nrV.AddItem FNVVRMCFiendEar 1\nSetFunctionValue 1\nendif\nEnd\n");
            dBad = !dScr; if (dBad) Log("Cyberware: no Fiend ears (FNVVRMC - Hunter.esp not loaded)");
        }
        typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
        struct { double num; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
        if (dScr && who && g_scriptIntf && g_scriptIntf->CallFunction && ((CallFn_t)g_scriptIntf->CallFunction)(dScr, GetPlayer(), NULL, &res, 1, who)
            && res.type == 1 && res.num > 0.5) { static int fl = 0; if (fl++ < 20) Log("Cyberware: a Fiend died - one Fiend Ear on the body"); }
    }
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
        LONG killsAll = kills;   /* (every kill counts for More RAM, Please - even the one that sets off Chrome Overload) */
        bool risk = g_cyEnd >= CY_PERK_END && (g_cyBorg || Cy_LoadUsed() > g_cyCapShown);
        if (hits && risk && !g_cyCO && now > g_cyCOReady)
            for (LONG i = 0; i < hits && !g_cyCO; i++) if (rand() % 100 < 2) Cy_CoStart();
        {   /* a kill: the chance is how far over capacity you are (20% over = 20% a kill) */
            int pct = Cy_OverPct();
            if (kills && risk && pct > 0 && !g_cyCO && now > g_cyCOReady) {
                for (LONG i = 0; i < kills && !g_cyCO; i++) if (rand() % 100 < pct) { Log("Cyberware: a kill set off Chrome Overload (%d%% over capacity)", pct); Cy_CoStart(); kills = 0; }
            }
        }
        if (killsAll && Cy_Has(CYS_RAM) && g_cyAbilReady > now) {   /* More RAM, Please: each kill 1 s off the ability's cooldown */
            double floor = g_cyAbil ? g_cyAbilEnd : now;
            g_cyAbilReady -= 1000.0 * (double)killsAll; if (g_cyAbilReady < floor) g_cyAbilReady = floor;
            Log("More RAM, Please: %ld kill%s - ability ready in %.1f s", (long)killsAll, killsAll == 1 ? "" : "s", (g_cyAbilReady - now) / 1000.0);
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
    Cy_DashTick();
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
    g_cyMantisN = 0; g_cyMantisStacks = 0; g_cyHkN = 0; g_cyRam = -1; g_cyHackOpen = false;
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
static int g_cySlot = 0, g_cyItem = 0, g_cyCol = 0, g_cyBtn = 0, g_cyTop = 0;   /* column 0 slots, 1 implants, 2 buttons */
static int g_cyHoverKind = 0, g_cyHoverIdx = -1;
static char g_cyMsg[200]; static double g_cyMsgUntil = 0;
static bool g_cyFromDoc = false;
static int Cy_SlotItems(int slot, const CyImpl **out, int max)
{
    int n = 0; for (int i = 0; i < CY_NCAT && n < max; i++) if (g_cyCat[i].slot == slot) {
        if (Cy_ArmSide(g_cyCat[i].id) == 2 && Cy_InstIdx(g_cyCat[i].id) < 0) continue;   /* (the old pairs: sold one arm at a time now) */
        out[n++] = &g_cyCat[i]; }
    return n;
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
#define CY_ROWS 5        /* implants shown at once - the arms have dozens: the list scrolls */
#define CY_LISTMAX 64
#define CY_MOREY0 (CY_Y0 + CY_ROWS * CY_ITH)
#define CY_MOREY1 (CY_MOREY0 + 56)
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
    Set_Text(g_setFontSmall, CY_DIM, 50, 76, SET_W - 40, 112, g_cyAtDoc ? "HUNTER  //  NEW VEGAS MEDICAL CLINIC" : g_cyAnywhere ? "RIPPERDOC ANYWHERE (TESTING) IS ON" : "VIEW ONLY  //  INSTALL AT A RIPPERDOC (OR TURN ON RIPPERDOC ANYWHERE)", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
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
    const CyImpl *it[CY_LISTMAX]; int ni = Cy_SlotItems(g_cySlot, it, CY_LISTMAX);
    if (g_cyItem >= ni) g_cyItem = ni - 1; if (g_cyItem < 0) g_cyItem = 0;
    if (g_cyItem < g_cyTop) g_cyTop = g_cyItem;   /* (the list scrolls to keep the picked one in view) */
    if (g_cyItem >= g_cyTop + CY_ROWS) g_cyTop = g_cyItem - CY_ROWS + 1;
    if (g_cyTop > ni - CY_ROWS) g_cyTop = ni - CY_ROWS; if (g_cyTop < 0) g_cyTop = 0;
    if (ni > CY_ROWS) {   /* more than fit: where you are in the list, and PREV / NEXT to page through it */
        int my0 = CY_MOREY0, my1 = CY_MOREY1 - 8, mid = (CY_RX0 + CY_RX1) / 2;
        bool hp = g_cyHoverKind == 4 && g_cyHoverIdx == 0, hn = g_cyHoverKind == 4 && g_cyHoverIdx == 1;
        Set_Fill(CY_RX0, my0, mid - 4, my1, hp ? RGB(18, 50, 54) : CY_PANEL); Set_Fill(mid + 4, my0, CY_RX1, my1, hn ? RGB(18, 50, 54) : CY_PANEL);
        Set_Text(g_setFontSmall, g_cyTop > 0 ? CY_TEAL : CY_DIM, CY_RX0 + 20, my0, mid - 20, my1, "<  PREVIOUS", DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        Set_Text(g_setFontSmall, g_cyTop + CY_ROWS < ni ? CY_TEAL : CY_DIM, mid + 20, my0, CY_RX1 - 20, my1, "NEXT  >", DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        snprintf(t, sizeof t, "%d-%d OF %d", g_cyTop + 1, g_cyTop + CY_ROWS, ni);
        Set_Text(g_setFontSmall, CY_DIM, CY_RX0, my0, CY_RX1, my1, t, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    for (int i = g_cyTop; i < ni && i < g_cyTop + CY_ROWS; i++) {
        int y0 = CY_Y0 + (i - g_cyTop) * CY_ITH, y1 = y0 + CY_ITH - 8;
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
                             c->special == CYS_DJUMP ? "ALWAYS ON: jump again in mid-air" : c->special == CYS_DASH ? "HOLD RIGHT GRIP + JUMP: dash (20 AP)" : c->special == CYS_RAM ? "ALWAYS ON: each kill -1 s ability cooldown" : c->special == CYS_BLOOD ? "ALWAYS ON: melee / unarmed hits heal 3%" : c->special == CYS_HACK ? "QUICKHACKS: scan with Cyber Eyes, right trigger uploads" : c->special == CYS_EYES ? "HOLD LEFT GRIP + LEFT TRIGGER: scan" : "ACTIVE ABILITY: squeeze both grips twice";
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
    const CyImpl *it[CY_LISTMAX]; int ni = Cy_SlotItems(g_cySlot, it, CY_LISTMAX);
    if (!ni) return;
    const CyImpl *c = it[g_cyItem < ni ? g_cyItem : 0];
    Cy_Say(btn == 0 ? Cy_Install(c) : Cy_Remove(c));
    Cy_Refresh();
}
/* where the aiming hand points on the panel (pixels); false off the panel. Also places the cursor. */
static bool Cy_Pointer(float *px, float *py)
{
    TrackedDevicePose_t ps[FNVR_MAXDEV]; int h = g_leftHanded ? 0 : 1; bool hit = false;
    g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, ps, FNVR_MAXDEV);
    if (g_handIdx[h] < FNVR_MAXDEV && ps[g_handIdx[h]].bPoseIsValid) {
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
    const CyImpl *it[CY_LISTMAX]; int ni = Cy_SlotItems(g_cySlot, it, CY_LISTMAX);
    float px = 0, py = 0; int hk = 0, hi = -1;
    if (Cy_Pointer(&px, &py)) {
        if (px >= CY_LX0 && px <= CY_LX1 && py >= CY_Y0 && py < CY_Y0 + CY_NSLOT * CY_SLH) { hk = 1; hi = (int)((py - CY_Y0) / CY_SLH); }
        else if (px >= CY_RX0 && px <= CY_RX1 && py >= CY_Y0 && py < CY_Y0 + (ni - g_cyTop < CY_ROWS ? ni - g_cyTop : CY_ROWS) * CY_ITH) { hk = 2; hi = g_cyTop + (int)((py - CY_Y0) / CY_ITH); }
        else if (ni > CY_ROWS && px >= CY_RX0 && px <= CY_RX1 && py >= CY_MOREY0 && py < CY_MOREY1) { hk = 4; hi = px < (CY_RX0 + CY_RX1) / 2 ? 0 : 1; }
        else if (py >= CY_BTNY0 && py <= CY_BTNY1 && px >= CY_RX0 && px <= CY_RX1) { hk = 3; hi = px < (CY_RX0 + CY_RX1) / 2 ? 0 : 1; }
    }
    if (hk != g_cyHoverKind || hi != g_cyHoverIdx) { g_cyHoverKind = hk; g_cyHoverIdx = hi; g_setDirty = true; if (hk) Set_Buzz(250); }
    if (trigEdge && hk) {
        if (hk == 1) { g_cySlot = hi; g_cyItem = 0; g_cyTop = 0; g_cyCol = 0; Set_Buzz(700); }
        else if (hk == 2) { g_cyItem = hi; g_cyCol = 1; Set_Buzz(700); }
        else if (hk == 3) { g_cyCol = 2; g_cyBtn = hi; Cy_Press(hi); }
        else if (hk == 4) {   /* a page back or on */
            int to = hi ? g_cyTop + CY_ROWS : g_cyTop - CY_ROWS; if (to > ni - CY_ROWS) to = ni - CY_ROWS; if (to < 0) to = 0;
            g_cyTop = to; g_cyItem = to; g_cyCol = 1; Set_Buzz(500);
        }
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
        if (g_cyCol == 0) { g_cySlot = (g_cySlot + nd + CY_NSLOT) % CY_NSLOT; g_cyItem = 0; g_cyTop = 0; }
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
