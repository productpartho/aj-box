/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * The 12 drum kits (PRD "Sound swaps", PRESETS). Kits 0..5 are the page kits.
 * Each edit is part, parameter (firmware descriptor order), value; AJB_PARTS
 * is the kit itself. Every kit keeps kit level 40 (100 -> 40, -8 dB headroom).
 *   BD  0 Level 1 Tone 2 Decay 3 Tune 4 Attack 5 Drive 6 Dist
 *   SD  0 Level 1 Tone 2 Snappy 3 Tune 4 Decay 5 Drive 6 Dist
 *   LT MT HT  0 Level 1 Tune 2 Decay 3 Sound (1 conga) 4 Drive 5 Dist
 *   RS  0 Level 1 Tune 2 Decay 3 Sound (1 clave) 4 Drive 5 Dist
 *   CP  0 Level 1 Tune 2 Decay 3 Attack 4 Sound (1 maracas) 5 Drive 6 Dist
 *   CB  0 Level 1 Tune 2 Decay 3 Drive 4 Dist
 *   CY OH  0 Level 1 Decay 2 Tune 3 Drive 4 Dist
 *   CH  0 Level 1 Tune 2 Decay 3 Drive 4 Dist
 * Dist: 0 diode, 1 clip, 2 saturate, 3 fuzz, 4 phase, 5 fold, 6 crush. */
#define KIT(id, label) static const ajb_kit_t kit_##id = {label, id##_edits, sizeof id##_edits / sizeof id##_edits[0]}

static const ajb_kit_edit_t pop_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 72}, {AJB_BD, 3, 70},             /* shorter, slightly higher kick */
    {AJB_SD, 2, 100}, {AJB_SD, 3, 74},            /* snappier, brighter snare */
    {AJB_CP, 0, 72},
    {AJB_CH, 0, 46}, {AJB_CH, 2, 40},             /* softer, tighter hats */
    {AJB_OH, 0, 44}, {AJB_OH, 1, 60},
    {AJB_CB, 0, 40},
};
KIT(pop, "Pop");

/* Tight punchy kick, crisp snare, loud clap and open hat. */
static const ajb_kit_edit_t disco_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 60}, {AJB_BD, 3, 76}, {AJB_BD, 4, 40},
    {AJB_SD, 1, 80}, {AJB_SD, 2, 110}, {AJB_SD, 3, 80}, {AJB_SD, 4, 90},
    {AJB_CP, 0, 80},
    {AJB_CH, 0, 48}, {AJB_CH, 2, 36},
    {AJB_OH, 0, 56}, {AJB_OH, 1, 66},
    {AJB_CB, 0, 44},
};
KIT(disco, "Disco");

/* Lo-fi: a low, long, warm kick; dusty crushed snare and hats. */
static const ajb_kit_edit_t headnod_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 30}, {AJB_BD, 2, 104}, {AJB_BD, 3, 52}, {AJB_BD, 5, 40}, {AJB_BD, 6, 2},
    {AJB_SD, 0, 52}, {AJB_SD, 1, 50}, {AJB_SD, 2, 70}, {AJB_SD, 3, 56}, {AJB_SD, 4, 96}, {AJB_SD, 5, 40}, {AJB_SD, 6, 6},
    {AJB_CP, 0, 66}, {AJB_CP, 1, 56},
    {AJB_RS, 0, 60},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 54}, {AJB_CH, 2, 34}, {AJB_CH, 3, 30}, {AJB_CH, 4, 6},
    {AJB_OH, 0, 40}, {AJB_OH, 1, 56},
};
KIT(headnod, "Head-Nod");

/* Hand drums: congas, clave, maracas, a boomy low drum and a timbale snare. */
static const ajb_kit_edit_t world_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 94}, {AJB_BD, 1, 34}, {AJB_BD, 2, 100}, {AJB_BD, 3, 54},
    {AJB_SD, 0, 30}, {AJB_SD, 1, 84}, {AJB_SD, 2, 50}, {AJB_SD, 3, 84}, {AJB_SD, 4, 80},
    {AJB_LT, 3, 1}, {AJB_MT, 3, 1}, {AJB_HT, 3, 1},
    {AJB_RS, 3, 1},
    {AJB_CP, 0, 70}, {AJB_CP, 4, 1},
    {AJB_CH, 0, 44}, {AJB_OH, 0, 44}, {AJB_CB, 0, 44},
};
KIT(world, "World");

/* Pots and pans: bucket kick, tin-can snare, high clanky congas and cowbell. */
static const ajb_kit_edit_t kitchen_edits[] = {
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
KIT(kitchen, "Kitchen");

/* Bedtime: a soft round kick, brushed snare, maracas, whispering hats. */
static const ajb_kit_edit_t sleepy_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 124}, {AJB_BD, 1, 24}, {AJB_BD, 2, 80}, {AJB_BD, 3, 54}, {AJB_BD, 4, 8},
    {AJB_SD, 0, 70}, {AJB_SD, 1, 30}, {AJB_SD, 2, 120}, {AJB_SD, 3, 60}, {AJB_SD, 4, 80},
    {AJB_RS, 0, 56},
    {AJB_CP, 0, 60}, {AJB_CP, 4, 1},
    {AJB_CY, 0, 40},
    {AJB_CH, 0, 44}, {AJB_CH, 2, 30},
    {AJB_OH, 0, 44}, {AJB_OH, 1, 50},
};
KIT(sleepy, "Sleepy");

/* Big and deep: a long sub kick, fat snare, booming toms. */
static const ajb_kit_edit_t boom_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 1, 30}, {AJB_BD, 2, 120}, {AJB_BD, 3, 48}, {AJB_BD, 5, 30}, {AJB_BD, 6, 2},
    {AJB_SD, 2, 96}, {AJB_SD, 3, 58}, {AJB_SD, 4, 118},
    {AJB_LT, 1, 50}, {AJB_LT, 2, 90}, {AJB_MT, 1, 50}, {AJB_MT, 2, 90}, {AJB_HT, 1, 50}, {AJB_HT, 2, 90},
    {AJB_CP, 0, 72},
    {AJB_CH, 0, 46}, {AJB_OH, 0, 44},
};
KIT(boom, "Boom");

/* Everything through the bit crusher. */
static const ajb_kit_edit_t robot_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 5, 60}, {AJB_BD, 6, 6},
    {AJB_SD, 3, 70}, {AJB_SD, 5, 60}, {AJB_SD, 6, 6},
    {AJB_LT, 4, 50}, {AJB_LT, 5, 6}, {AJB_MT, 4, 50}, {AJB_MT, 5, 6}, {AJB_HT, 4, 50}, {AJB_HT, 5, 6},
    {AJB_CP, 5, 50}, {AJB_CP, 6, 6},
    {AJB_CB, 0, 44}, {AJB_CB, 3, 40}, {AJB_CB, 4, 6},
    {AJB_CH, 0, 46}, {AJB_CH, 3, 50}, {AJB_CH, 4, 6},
    {AJB_OH, 0, 44}, {AJB_OH, 3, 50}, {AJB_OH, 4, 6},
};
KIT(robot, "Robot");

/* A toy kit: everything tuned up and cut short. */
static const ajb_kit_edit_t tiny_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 40}, {AJB_BD, 3, 100},
    {AJB_SD, 0, 45}, {AJB_SD, 3, 100}, {AJB_SD, 4, 60},
    {AJB_LT, 1, 100}, {AJB_LT, 2, 36}, {AJB_MT, 1, 100}, {AJB_MT, 2, 36}, {AJB_HT, 1, 100}, {AJB_HT, 2, 36},
    {AJB_RS, 1, 90},
    {AJB_CP, 1, 90}, {AJB_CP, 2, 40},
    {AJB_CB, 0, 40}, {AJB_CB, 1, 90},
    {AJB_CH, 0, 46}, {AJB_CH, 1, 90}, {AJB_CH, 2, 30},
    {AJB_OH, 0, 44}, {AJB_OH, 1, 44}, {AJB_OH, 2, 90},
};
KIT(tiny, "Tiny");

/* Fuzz kick, folded snare and toms. */
static const ajb_kit_edit_t fuzzy_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 80}, {AJB_BD, 5, 70}, {AJB_BD, 6, 3},
    {AJB_SD, 0, 76}, {AJB_SD, 5, 60}, {AJB_SD, 6, 5},
    {AJB_LT, 4, 60}, {AJB_LT, 5, 5}, {AJB_MT, 4, 60}, {AJB_MT, 5, 5}, {AJB_HT, 4, 60}, {AJB_HT, 5, 5},
    {AJB_CP, 5, 40}, {AJB_CP, 6, 2},
    {AJB_CH, 0, 44}, {AJB_OH, 0, 42},
};
KIT(fuzzy, "Fuzzy");

/* Bongos and congas, a dry high snare, clave and maracas. */
static const ajb_kit_edit_t bongo_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 0, 76}, {AJB_BD, 1, 50}, {AJB_BD, 2, 46}, {AJB_BD, 3, 80},
    {AJB_SD, 0, 20}, {AJB_SD, 1, 70}, {AJB_SD, 2, 30}, {AJB_SD, 3, 88}, {AJB_SD, 4, 60},
    {AJB_LT, 1, 76}, {AJB_LT, 3, 1}, {AJB_MT, 1, 80}, {AJB_MT, 3, 1}, {AJB_HT, 1, 84}, {AJB_HT, 3, 1},
    {AJB_RS, 3, 1},
    {AJB_CP, 4, 1},
    {AJB_CH, 0, 40}, {AJB_OH, 0, 40},
};
KIT(bongo, "Bongo");

/* Zaps and long tails: a swooping kick, ringing metal, toms spread far apart. */
static const ajb_kit_edit_t space_edits[] = {
    {AJB_PARTS, 0, 40},
    {AJB_BD, 2, 110}, {AJB_BD, 3, 40}, {AJB_BD, 5, 40}, {AJB_BD, 6, 4},
    {AJB_SD, 0, 40}, {AJB_SD, 2, 60}, {AJB_SD, 3, 90}, {AJB_SD, 4, 120},
    {AJB_LT, 1, 40}, {AJB_LT, 2, 100}, {AJB_MT, 2, 100}, {AJB_HT, 1, 100}, {AJB_HT, 2, 100},
    {AJB_CP, 1, 80}, {AJB_CP, 2, 100},
    {AJB_CB, 0, 44}, {AJB_CB, 1, 40}, {AJB_CB, 2, 100},
    {AJB_CY, 1, 110}, {AJB_CY, 2, 90},
    {AJB_CH, 0, 44}, {AJB_CH, 1, 100}, {AJB_CH, 2, 60},
    {AJB_OH, 0, 40}, {AJB_OH, 1, 100}, {AJB_OH, 2, 100},
};
KIT(space, "Space");

static const ajb_kit_t *const kits[] = {
    &kit_pop, &kit_disco, &kit_headnod, &kit_world, &kit_kitchen, &kit_sleepy,
    &kit_boom, &kit_robot, &kit_tiny, &kit_fuzzy, &kit_bongo, &kit_space,
};
