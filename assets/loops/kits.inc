/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Drum kits (PRD "Sound swaps", PRESETS): the six home kits the pages start
 * on, and the 12 kits PRESETS steps through.
 *
 * An 808 edit is part, parameter (firmware descriptor order), value; AJB_PARTS
 * is the kit itself. Every kit keeps kit level 40 (the stock app opens it up).
 *   BD  0 Level 1 Tone 2 Decay 3 Tune 4 Attack 5 Drive 6 Dist
 *   SD  0 Level 1 Tone 2 Snappy 3 Tune 4 Decay 5 Drive 6 Dist 7 Rev 8 Dly
 *   LT MT HT  0 Level 1 Tune 2 Decay 3 Sound (1 conga) 4 Drive 5 Dist 6 Rev 7 Dly
 *   RS  0 Level 1 Tune 2 Decay 3 Sound (1 clave) 4 Drive 5 Dist 6 Rev 7 Dly
 *   CP  0 Level 1 Tune 2 Decay 3 Attack 4 Sound (1 maracas) 5 Drive 6 Dist 7 Rev 8 Dly
 *   CB  0 Level 1 Tune 2 Decay 3 Drive 4 Dist 5 Rev 6 Dly
 *   CY OH  0 Level 1 Decay 2 Tune 3 Drive 4 Dist 5 Rev 6 Dly
 *   CH  0 Level 1 Tune 2 Decay 3 Drive 4 Dist 5 Rev 6 Dly
 * Dist: 0 diode, 1 clip, 2 saturate, 3 fuzz, 4 phase, 5 fold, 6 crush.
 *
 * A pad edit is pad, parameter, value: 0 Level (100), 1 Tune (24, one per
 * semitone, 0..48), 2 Length (127). Pads (dsp/stock/oneshots.h): 0 dhol low,
 * 1 dhol high, 2 pot, 3 spoon, 4 mouth kick, 5 "shh", 6 cuica high, 7 cuica low. */
#define EDITS(a) a, sizeof a / sizeof a[0]
#define PART(x) (1U << AJB_##x)
#define PAD(n) (1U << (n))

/* ---- the home kits: what each page's drum loops sound like until PRESETS is
 * turned. Unchanged since the first build; 808 settings only, every line on
 * its own sound. Order: pages[].kit in loops.c. ---- */

static const ajb_kit_edit_t home_pop_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 72}, {AJB_BD, 3, 70},             /* shorter, slightly higher kick */
    {AJB_SD, 2, 100}, {AJB_SD, 3, 74},            /* snappier, brighter snare */
    {AJB_CP, 0, 72},
    {AJB_CH, 0, 46}, {AJB_CH, 2, 40},             /* softer, tighter hats */
    {AJB_OH, 0, 44}, {AJB_OH, 1, 60},
    {AJB_CB, 0, 40},
};
static const ajb_kit_t home_pop = {"Pop", EDITS(home_pop_edits)};

/* Tight punchy kick, crisp snare, loud clap and open hat. */
static const ajb_kit_edit_t home_disco_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 60}, {AJB_BD, 3, 76}, {AJB_BD, 4, 40},
    {AJB_SD, 1, 80}, {AJB_SD, 2, 110}, {AJB_SD, 3, 80}, {AJB_SD, 4, 90},
    {AJB_CP, 0, 80},
    {AJB_CH, 0, 48}, {AJB_CH, 2, 36},
    {AJB_OH, 0, 56}, {AJB_OH, 1, 66},
    {AJB_CB, 0, 44},
};
static const ajb_kit_t home_disco = {"Disco", EDITS(home_disco_edits)};

/* Lo-fi: a low, long, warm kick; dusty crushed snare and hats. */
static const ajb_kit_edit_t home_headnod_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 30}, {AJB_BD, 2, 104}, {AJB_BD, 3, 52}, {AJB_BD, 5, 40}, {AJB_BD, 6, 2},
    {AJB_SD, 0, 52}, {AJB_SD, 1, 50}, {AJB_SD, 2, 70}, {AJB_SD, 3, 56}, {AJB_SD, 4, 96}, {AJB_SD, 5, 40}, {AJB_SD, 6, 6},
    {AJB_CP, 0, 66}, {AJB_CP, 1, 56},
    {AJB_RS, 0, 60},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 54}, {AJB_CH, 2, 34}, {AJB_CH, 3, 30}, {AJB_CH, 4, 6},
    {AJB_OH, 0, 40}, {AJB_OH, 1, 56},
};
static const ajb_kit_t home_headnod = {"Head-Nod", EDITS(home_headnod_edits)};

/* Hand drums: congas, clave, maracas, a boomy low drum and a timbale snare. */
static const ajb_kit_edit_t home_world_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 94}, {AJB_BD, 1, 34}, {AJB_BD, 2, 100}, {AJB_BD, 3, 54},
    {AJB_SD, 0, 30}, {AJB_SD, 1, 84}, {AJB_SD, 2, 50}, {AJB_SD, 3, 84}, {AJB_SD, 4, 80},
    {AJB_LT, 3, 1}, {AJB_MT, 3, 1}, {AJB_HT, 3, 1},
    {AJB_RS, 3, 1},
    {AJB_CP, 0, 70}, {AJB_CP, 4, 1},
    {AJB_CH, 0, 44}, {AJB_OH, 0, 44}, {AJB_CB, 0, 44},
};
static const ajb_kit_t home_world = {"World", EDITS(home_world_edits)};

/* Pots and pans: bucket kick, tin-can snare, high clanky congas and cowbell. */
static const ajb_kit_edit_t home_kitchen_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 70}, {AJB_BD, 2, 50}, {AJB_BD, 3, 84},
    {AJB_SD, 0, 22}, {AJB_SD, 1, 90}, {AJB_SD, 2, 40}, {AJB_SD, 3, 96}, {AJB_SD, 4, 60},
    {AJB_LT, 1, 84}, {AJB_LT, 2, 40}, {AJB_LT, 3, 1},
    {AJB_MT, 1, 88}, {AJB_MT, 2, 40}, {AJB_MT, 3, 1},
    {AJB_HT, 1, 92}, {AJB_HT, 2, 40}, {AJB_HT, 3, 1},
    {AJB_RS, 1, 80}, {AJB_RS, 3, 1},
    {AJB_CP, 4, 1},
    {AJB_CB, 0, 56}, {AJB_CB, 1, 80},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 80}, {AJB_CH, 2, 30},
    {AJB_OH, 0, 40}, {AJB_OH, 1, 50}, {AJB_OH, 2, 76},
};
static const ajb_kit_t home_kitchen = {"Kitchen", EDITS(home_kitchen_edits)};

/* Bedtime: a soft round kick, brushed snare, maracas, whispering hats. */
static const ajb_kit_edit_t home_sleepy_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 124}, {AJB_BD, 1, 24}, {AJB_BD, 2, 80}, {AJB_BD, 3, 54}, {AJB_BD, 4, 8},
    {AJB_SD, 0, 70}, {AJB_SD, 1, 30}, {AJB_SD, 2, 120}, {AJB_SD, 3, 60}, {AJB_SD, 4, 80},
    {AJB_RS, 0, 56},
    {AJB_CP, 0, 60}, {AJB_CP, 4, 1},
    {AJB_CY, 0, 40},
    {AJB_CH, 0, 44}, {AJB_CH, 2, 30},
    {AJB_OH, 0, 44}, {AJB_OH, 1, 50},
};
static const ajb_kit_t home_sleepy = {"Sleepy", EDITS(home_sleepy_edits)};

static const ajb_kit_t *const home_kits[] = {
    &home_pop, &home_disco, &home_headnod, &home_world, &home_kitchen, &home_sleepy,
};

/* ---- the 12 PRESETS kits (Nov 22, 2026). The first twelve were all the 808
 * with small changes and sounded alike (heard on the FM-1; `ajstock kits`
 * measures it). Each of these is a different set of sounds: it says which 808
 * drums and which recorded sounds play every line of a loop, so a loop of
 * recorded sounds changes with the kit too. Order: kit_art in ajscreen.c. ---- */

/* A recorded-sound line on the drum machine: dhol low, pot and mouth kick on
 * the kick, dhol high and the cuicas on the toms, spoon on the rim, "shh" on
 * the snare. */
static const ajb_kit_route_t pads_on_808[AJB_PADS] = {
    [0] = {PART(BD), 0}, [1] = {PART(HT), 0}, [2] = {PART(BD), 0}, [3] = {PART(RS), 0},
    [4] = {PART(BD), 0}, [5] = {PART(SD), 0}, [6] = {PART(HT), 0}, [7] = {PART(MT), 0},
};

/* 1 Pop: the drum machine, clean and bright. */
static const ajb_kit_edit_t pop_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 72}, {AJB_BD, 3, 70},
    {AJB_SD, 2, 100}, {AJB_SD, 3, 74},
    {AJB_CP, 0, 72},
    {AJB_CH, 0, 46}, {AJB_CH, 2, 40},
    {AJB_OH, 0, 44}, {AJB_OH, 1, 60},
    {AJB_CB, 0, 40},
};
static const ajb_kit_t kit_pop = {"Pop", EDITS(pop_edits), NULL, 0, NULL, pads_on_808};

/* 2 Disco: every snare is a snare and a clap, the claps are big and bright
 * with reverb, every hat is an open hat and the open hat a cymbal, the toms
 * are short dry congas, the rim is a cowbell, the kick short and clicky. */
static const ajb_kit_edit_t disco_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 60}, {AJB_BD, 2, 56}, {AJB_BD, 3, 84}, {AJB_BD, 4, 70},
    {AJB_SD, 0, 56}, {AJB_SD, 1, 90}, {AJB_SD, 2, 115}, {AJB_SD, 3, 84}, {AJB_SD, 4, 80},
    {AJB_LT, 2, 40}, {AJB_LT, 3, 1}, {AJB_MT, 2, 30}, {AJB_MT, 3, 1}, {AJB_HT, 2, 30}, {AJB_HT, 3, 1},
    {AJB_CP, 0, 80}, {AJB_CP, 1, 110}, {AJB_CP, 2, 100}, {AJB_CP, 7, 60},
    {AJB_CB, 0, 48}, {AJB_CB, 1, 76},
    {AJB_CY, 0, 50}, {AJB_CY, 1, 60},
    {AJB_OH, 0, 50}, {AJB_OH, 1, 50}, {AJB_OH, 2, 80},
};
static const ajb_kit_route_t disco_part_to[AJB_PARTS] = {
    [AJB_SD] = {PART(SD) | PART(CP), 0}, [AJB_RS] = {PART(CB), 0}, [AJB_CH] = {PART(OH), 0}, [AJB_OH] = {PART(CY), 0},
};
static const ajb_kit_route_t disco_pad_to[AJB_PADS] = {
    [0] = {PART(BD), 0}, [1] = {PART(HT), 0}, [2] = {PART(BD), 0}, [3] = {PART(OH), 0},
    [4] = {PART(BD), 0}, [5] = {PART(SD) | PART(CP), 0}, [6] = {PART(HT), 0}, [7] = {PART(MT), 0},
};
static const ajb_kit_t kit_disco = {"Disco", EDITS(disco_edits), NULL, 0, disco_part_to, disco_pad_to};

/* 3 Head-Nod: a beatbox. Mouth kick for the kick and the low tom, "shh" for
 * the snare, the spoon as a finger snap for the clap; the other toms are a
 * low rim "k" and a dull "ts" hat. */
static const ajb_kit_edit_t headnod_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_RS, 0, 56}, {AJB_RS, 1, 30},
    {AJB_CB, 0, 40}, {AJB_CB, 1, 40},
    {AJB_CY, 0, 40},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 40}, {AJB_CH, 2, 30},
    {AJB_OH, 0, 36}, {AJB_OH, 1, 50}, {AJB_OH, 2, 40},
};
static const ajb_kit_edit_t headnod_pads[] = {{4, 0, 110}};
static const ajb_kit_route_t headnod_part_to[AJB_PARTS] = {
    [AJB_BD] = {0, PAD(4)}, [AJB_SD] = {0, PAD(5)}, [AJB_CP] = {0, PAD(3)},
    [AJB_LT] = {0, PAD(4)}, [AJB_MT] = {PART(RS), 0}, [AJB_HT] = {PART(CH), 0},
};
static const ajb_kit_route_t headnod_pad_to[AJB_PADS] = {
    [0] = {0, PAD(4)}, [1] = {PART(CH), 0}, [2] = {0, PAD(4)}, [6] = {PART(CH), 0}, [7] = {PART(RS), 0},
};
static const ajb_kit_t kit_headnod = {"Head-Nod", EDITS(headnod_edits), EDITS(headnod_pads), headnod_part_to, headnod_pad_to};

/* 4 World: the dhol plays kick, snare and the outer toms; a conga, clave and
 * maracas the rest. */
static const ajb_kit_edit_t world_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_LT, 3, 1}, {AJB_MT, 3, 1}, {AJB_HT, 3, 1},
    {AJB_RS, 3, 1},
    {AJB_CP, 0, 70}, {AJB_CP, 4, 1},
    {AJB_CB, 0, 44},
};
static const ajb_kit_route_t world_part_to[AJB_PARTS] = {
    [AJB_BD] = {0, PAD(0)}, [AJB_SD] = {0, PAD(1)}, [AJB_LT] = {0, PAD(0)}, [AJB_HT] = {0, PAD(1)},
    [AJB_CH] = {PART(CP), 0}, [AJB_OH] = {PART(CP), 0},
};
static const ajb_kit_route_t world_pad_to[AJB_PADS] = {
    [2] = {0, PAD(0)}, [3] = {PART(RS), 0}, [4] = {0, PAD(0)}, [5] = {PART(CP), 0},
    [6] = {PART(HT), 0}, [7] = {PART(MT), 0},
};
static const ajb_kit_t kit_world = {"World", EDITS(world_edits), NULL, 0, world_part_to, world_pad_to};

/* 5 Kitchen: the pot for the kick and the low tom, the spoon for the snare;
 * claps and the other toms are high clanky bowls (congas), hats a glass "tink"
 * (a high clave), the open hat a pan lid (a high cowbell). */
static const ajb_kit_edit_t kitchen_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_LT, 1, 127}, {AJB_LT, 2, 40}, {AJB_LT, 3, 1},
    {AJB_MT, 1, 127}, {AJB_MT, 2, 40}, {AJB_MT, 3, 1},
    {AJB_HT, 1, 127}, {AJB_HT, 2, 40}, {AJB_HT, 3, 1},
    {AJB_RS, 0, 50}, {AJB_RS, 1, 90}, {AJB_RS, 3, 1},
    {AJB_CB, 0, 50}, {AJB_CB, 1, 100}, {AJB_CB, 2, 40},
    {AJB_CY, 0, 44},
};
static const ajb_kit_edit_t kitchen_pads[] = {{2, 1, 20}};
static const ajb_kit_route_t kitchen_part_to[AJB_PARTS] = {
    [AJB_BD] = {0, PAD(2)}, [AJB_SD] = {0, PAD(3)}, [AJB_CP] = {PART(HT), 0}, [AJB_LT] = {0, PAD(2)},
    [AJB_CH] = {PART(RS), 0}, [AJB_OH] = {PART(CB), 0},
};
static const ajb_kit_route_t kitchen_pad_to[AJB_PADS] = {
    [0] = {0, PAD(2)}, [1] = {0, PAD(3)}, [4] = {0, PAD(2)}, [5] = {0, PAD(3)},
    [6] = {PART(HT), 0}, [7] = {PART(MT), 0},
};
static const ajb_kit_t kit_kitchen = {"Kitchen", EDITS(kitchen_edits), EDITS(kitchen_pads), kitchen_part_to, kitchen_pad_to};

/* 6 Sleepy: a heartbeat. The kick is a soft low tom, the snare brushed, the
 * hats a short high hush (the "shh" an octave up), claps maracas, the other toms a soft rim and
 * the hush, and the room's reverb on everything. */
static const ajb_kit_edit_t sleepy_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_SD, 0, 60}, {AJB_SD, 1, 30}, {AJB_SD, 2, 127}, {AJB_SD, 3, 50}, {AJB_SD, 4, 90}, {AJB_SD, 7, 70},
    {AJB_LT, 0, 70}, {AJB_LT, 1, 40}, {AJB_LT, 2, 80}, {AJB_LT, 6, 40},
    {AJB_RS, 0, 50}, {AJB_RS, 6, 60},
    {AJB_CP, 0, 56}, {AJB_CP, 4, 1}, {AJB_CP, 7, 40},
    {AJB_CY, 0, 36}, {AJB_CY, 5, 60},
};
static const ajb_kit_edit_t sleepy_pads[] = {{5, 0, 60}, {5, 1, 36}, {5, 2, 40}};
static const ajb_kit_route_t sleepy_part_to[AJB_PARTS] = {
    [AJB_BD] = {PART(LT), 0}, [AJB_MT] = {PART(RS), 0}, [AJB_HT] = {0, PAD(5)}, [AJB_CB] = {PART(RS), 0},
    [AJB_CH] = {0, PAD(5)}, [AJB_OH] = {0, PAD(5)},
};
static const ajb_kit_route_t sleepy_pad_to[AJB_PADS] = {
    [0] = {PART(LT), 0}, [1] = {0, PAD(5)}, [2] = {PART(LT), 0}, [3] = {0, PAD(5)},
    [4] = {PART(LT), 0}, [6] = {PART(RS), 0}, [7] = {PART(LT), 0},
};
static const ajb_kit_t kit_sleepy = {"Sleepy", EDITS(sleepy_edits), EDITS(sleepy_pads), sleepy_part_to, sleepy_pad_to};

/* 7 Boom: giant drums. The kick is the 808's longest and lowest with the dhol
 * an octave down on top; every snare is a deep snare and a low tom; the toms
 * are long and driven, the clap a low thunder clap, the hats low, the cymbal a
 * gong; reverb on the lot. */
static const ajb_kit_edit_t boom_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 50}, {AJB_BD, 2, 124}, {AJB_BD, 3, 40}, {AJB_BD, 4, 50}, {AJB_BD, 5, 50}, {AJB_BD, 6, 2},
    {AJB_SD, 0, 60}, {AJB_SD, 1, 30}, {AJB_SD, 2, 70}, {AJB_SD, 3, 36}, {AJB_SD, 4, 127}, {AJB_SD, 7, 60},
    {AJB_LT, 1, 0}, {AJB_LT, 2, 110}, {AJB_LT, 4, 100}, {AJB_LT, 5, 2}, {AJB_LT, 6, 50},
    {AJB_MT, 1, 0}, {AJB_MT, 2, 100}, {AJB_MT, 4, 100}, {AJB_MT, 5, 2}, {AJB_MT, 6, 50},
    {AJB_HT, 1, 0}, {AJB_HT, 2, 96}, {AJB_HT, 4, 100}, {AJB_HT, 5, 2}, {AJB_HT, 6, 50},
    {AJB_RS, 1, 24},
    {AJB_CP, 1, 24}, {AJB_CP, 2, 110}, {AJB_CP, 7, 70},
    {AJB_CB, 0, 40}, {AJB_CB, 1, 24},
    {AJB_CY, 0, 48}, {AJB_CY, 1, 110}, {AJB_CY, 2, 20}, {AJB_CY, 5, 60},
    {AJB_CH, 0, 36}, {AJB_CH, 1, 24},
    {AJB_OH, 0, 36}, {AJB_OH, 1, 60}, {AJB_OH, 2, 24},
};
static const ajb_kit_edit_t boom_pads[] = {{0, 1, 12}};
static const ajb_kit_route_t boom_part_to[AJB_PARTS] = {
    [AJB_BD] = {PART(BD), PAD(0)}, [AJB_SD] = {PART(SD) | PART(LT), 0},
};
static const ajb_kit_route_t boom_pad_to[AJB_PADS] = {
    [0] = {PART(BD), PAD(0)}, [1] = {PART(MT), 0}, [2] = {PART(BD), PAD(0)}, [3] = {PART(RS), 0},
    [4] = {PART(BD), PAD(0)}, [5] = {PART(SD) | PART(LT), 0}, [6] = {PART(HT), 0}, [7] = {PART(LT), 0},
};
static const ajb_kit_t kit_boom = {"Boom", EDITS(boom_edits), EDITS(boom_pads), boom_part_to, boom_pad_to};

/* 8 Robot: everything through the bit crusher, and beeps: the hats are a
 * high clave, snares carry a cowbell and claps are one, the open hat is a
 * short crushed cymbal. */
static const ajb_kit_edit_t robot_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 3, 70}, {AJB_BD, 5, 80}, {AJB_BD, 6, 6},
    {AJB_SD, 2, 60}, {AJB_SD, 3, 90}, {AJB_SD, 5, 80}, {AJB_SD, 6, 6},
    {AJB_LT, 4, 80}, {AJB_LT, 5, 6}, {AJB_MT, 4, 80}, {AJB_MT, 5, 6}, {AJB_HT, 4, 80}, {AJB_HT, 5, 6},
    {AJB_RS, 0, 56}, {AJB_RS, 1, 100}, {AJB_RS, 2, 50}, {AJB_RS, 3, 1},
    {AJB_CB, 0, 50}, {AJB_CB, 1, 100}, {AJB_CB, 2, 40}, {AJB_CB, 3, 60}, {AJB_CB, 4, 6},
    {AJB_CY, 0, 50}, {AJB_CY, 1, 40}, {AJB_CY, 3, 70}, {AJB_CY, 4, 6},
};
static const ajb_kit_route_t robot_part_to[AJB_PARTS] = {
    [AJB_SD] = {PART(SD) | PART(CB), 0}, [AJB_CP] = {PART(CB), 0}, [AJB_CH] = {PART(RS), 0}, [AJB_OH] = {PART(CY), 0},
};
static const ajb_kit_route_t robot_pad_to[AJB_PADS] = {
    [0] = {PART(BD), 0}, [1] = {PART(RS), 0}, [2] = {PART(BD), 0}, [3] = {PART(RS), 0},
    [4] = {PART(BD), 0}, [5] = {PART(SD) | PART(CB), 0}, [6] = {PART(HT), 0}, [7] = {PART(MT), 0},
};
static const ajb_kit_t kit_robot = {"Robot", EDITS(robot_edits), NULL, 0, robot_part_to, robot_pad_to};

/* 9 Tiny: a toy kit. Every drum is tuned as high and cut as short as it
 * goes; the snare is a rim click and the spoon, the toms a cowbell, a rim and
 * the pot; and the recorded sounds play an octave up. */
static const ajb_kit_edit_t tiny_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 70}, {AJB_BD, 1, 90}, {AJB_BD, 2, 20}, {AJB_BD, 3, 127}, {AJB_BD, 4, 90},
    {AJB_RS, 1, 127}, {AJB_RS, 2, 30},
    {AJB_CP, 0, 60}, {AJB_CP, 1, 127}, {AJB_CP, 2, 20},
    {AJB_CB, 0, 44}, {AJB_CB, 1, 127}, {AJB_CB, 2, 20},
    {AJB_CY, 0, 40}, {AJB_CY, 1, 30}, {AJB_CY, 2, 127},
    {AJB_CH, 0, 46}, {AJB_CH, 1, 127}, {AJB_CH, 2, 20},
    {AJB_OH, 0, 44}, {AJB_OH, 1, 30}, {AJB_OH, 2, 127},
};
static const ajb_kit_edit_t tiny_pads[] = {
    {0, 1, 36}, {1, 1, 36}, {2, 1, 36}, {3, 1, 36}, {4, 1, 36}, {5, 1, 36}, {6, 1, 36}, {7, 1, 36},
    {0, 2, 60}, {1, 2, 60}, {2, 2, 60}, {4, 2, 60}, {5, 2, 60},
};
static const ajb_kit_route_t tiny_part_to[AJB_PARTS] = {
    [AJB_SD] = {PART(RS), PAD(3)}, [AJB_LT] = {PART(CB), 0}, [AJB_MT] = {PART(RS), 0}, [AJB_HT] = {0, PAD(2)},
};
static const ajb_kit_t kit_tiny = {"Tiny", EDITS(tiny_edits), EDITS(tiny_pads), tiny_part_to, NULL};

/* 10 Fuzzy: a rock kit through fuzz. Kick, rim and cowbell fuzzed, the snare
 * long and folded with a folded tom under it, the toms folded, every clap a snare, and every
 * closed hat a short cymbal. */
static const ajb_kit_edit_t fuzzy_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 80}, {AJB_BD, 2, 50}, {AJB_BD, 3, 90}, {AJB_BD, 5, 100}, {AJB_BD, 6, 3},
    {AJB_SD, 0, 76}, {AJB_SD, 1, 80}, {AJB_SD, 4, 120}, {AJB_SD, 5, 90}, {AJB_SD, 6, 5},
    {AJB_LT, 2, 80}, {AJB_LT, 4, 110}, {AJB_LT, 5, 5}, {AJB_MT, 2, 70}, {AJB_MT, 4, 110}, {AJB_MT, 5, 5},
    {AJB_HT, 2, 66}, {AJB_HT, 4, 110}, {AJB_HT, 5, 5},
    {AJB_RS, 4, 80}, {AJB_RS, 5, 3},
    {AJB_CB, 0, 44}, {AJB_CB, 3, 80}, {AJB_CB, 4, 3},
    {AJB_CY, 0, 40}, {AJB_CY, 1, 20}, {AJB_CY, 3, 50}, {AJB_CY, 4, 2},
    {AJB_OH, 0, 44}, {AJB_OH, 3, 80}, {AJB_OH, 4, 3},
};
static const ajb_kit_route_t fuzzy_part_to[AJB_PARTS] = {
    [AJB_SD] = {PART(SD) | PART(HT), 0}, [AJB_CP] = {PART(SD), 0}, [AJB_CH] = {PART(CY), 0},
};
static const ajb_kit_route_t fuzzy_pad_to[AJB_PADS] = {
    [0] = {PART(BD), 0}, [1] = {PART(MT), 0}, [2] = {PART(BD), 0}, [3] = {PART(RS), 0},
    [4] = {PART(BD), 0}, [5] = {PART(SD) | PART(HT), 0}, [6] = {PART(HT), 0}, [7] = {PART(LT), 0},
};
static const ajb_kit_t kit_fuzzy = {"Fuzzy", EDITS(fuzzy_edits), NULL, 0, fuzzy_part_to, fuzzy_pad_to};

/* 11 Bongo: no kick or snare at all. Low conga for the kick, high conga
 * for the snare, the cuicas on the low and middle tom lines, a clave for the
 * claps and the hats and a high cowbell for the open hat. */
static const ajb_kit_edit_t bongo_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_LT, 1, 30}, {AJB_LT, 2, 70}, {AJB_LT, 3, 1},
    {AJB_MT, 3, 1},
    {AJB_HT, 1, 100}, {AJB_HT, 2, 50}, {AJB_HT, 3, 1},
    {AJB_RS, 3, 1},
    {AJB_CP, 0, 60}, {AJB_CP, 4, 1},
    {AJB_CB, 0, 50}, {AJB_CB, 1, 100}, {AJB_CB, 2, 40},
    /* Congas and clave are quiet next to a kick: all up together, after the Sound switches above. */
    {AJB_LT, 0, 110}, {AJB_MT, 0, 110}, {AJB_HT, 0, 110}, {AJB_RS, 0, 100}, {AJB_CP, 0, 90}, {AJB_CB, 0, 76},
};
static const ajb_kit_route_t bongo_part_to[AJB_PARTS] = {
    [AJB_BD] = {PART(LT), 0}, [AJB_SD] = {PART(HT), 0}, [AJB_LT] = {0, PAD(7)}, [AJB_MT] = {0, PAD(6)},
    [AJB_HT] = {PART(MT), 0}, [AJB_CP] = {PART(RS), 0}, [AJB_CH] = {PART(RS), 0}, [AJB_OH] = {PART(CB), 0},
};
static const ajb_kit_route_t bongo_pad_to[AJB_PADS] = {
    [0] = {PART(LT), 0}, [1] = {PART(HT), 0}, [2] = {PART(LT), 0}, [3] = {PART(RS), 0},
    [4] = {PART(LT), 0}, [5] = {PART(MT), 0},
};
static const ajb_kit_t kit_bongo = {"Bongo", EDITS(bongo_edits), NULL, 0, bongo_part_to, bongo_pad_to};

/* 12 Space: zaps and echoes. The kick swoops down, everything rings long
 * into the echo and the reverb, the low and middle toms are the cuicas an
 * octave down, the high tom a clave ping and the clap a low ringing cowbell,
 * and the recorded sounds play an octave down. */
static const ajb_kit_edit_t space_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 70}, {AJB_BD, 2, 127}, {AJB_BD, 3, 30}, {AJB_BD, 4, 100}, {AJB_BD, 5, 50}, {AJB_BD, 6, 4},
    {AJB_SD, 0, 50}, {AJB_SD, 1, 100}, {AJB_SD, 2, 40}, {AJB_SD, 3, 127}, {AJB_SD, 4, 127}, {AJB_SD, 7, 40}, {AJB_SD, 8, 80},
    {AJB_RS, 1, 127}, {AJB_RS, 2, 90}, {AJB_RS, 3, 1}, {AJB_RS, 7, 80},
    {AJB_CB, 0, 44}, {AJB_CB, 1, 20}, {AJB_CB, 2, 110}, {AJB_CB, 6, 70},
    {AJB_CY, 1, 127}, {AJB_CY, 2, 100}, {AJB_CY, 5, 80},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 127}, {AJB_CH, 2, 70}, {AJB_CH, 6, 70},
    {AJB_OH, 0, 40}, {AJB_OH, 1, 110}, {AJB_OH, 2, 127}, {AJB_OH, 5, 60}, {AJB_OH, 6, 60},
};
static const ajb_kit_edit_t space_pads[] = {
    {0, 1, 12}, {1, 1, 12}, {2, 1, 12}, {3, 1, 12}, {4, 1, 12}, {5, 1, 12}, {6, 1, 12}, {7, 1, 12},
};
static const ajb_kit_route_t space_part_to[AJB_PARTS] = {
    [AJB_LT] = {0, PAD(7)}, [AJB_MT] = {0, PAD(6)}, [AJB_HT] = {PART(RS), 0}, [AJB_CP] = {PART(CB), 0},
};
static const ajb_kit_t kit_space = {"Space", EDITS(space_edits), EDITS(space_pads), space_part_to, NULL};

static const ajb_kit_t *const kits[] = {
    &kit_pop, &kit_disco, &kit_headnod, &kit_world, &kit_kitchen, &kit_sleepy,
    &kit_boom, &kit_robot, &kit_tiny, &kit_fuzzy, &kit_bongo, &kit_space,
};
