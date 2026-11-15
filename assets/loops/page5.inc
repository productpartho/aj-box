/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 5: Silly Kitchen drums (pots, spoons, beatbox), Game On melodies (chiptune). */

/* The two sampled kitchen loops, one slice per 16th. */
static const ajb_drum_loop_t pots_and_pans = {"Pots and Pans", 16, AJB_SLICED(pots), {0}, GAIN(0.36), {
    [2] = "X...x..x..X.x...",      /* pot lid */
    [3] = "..xx.xx.xx.x.xxx",      /* spoons */
}};
static const ajb_drum_loop_t beatbox = {"Beatbox", 16, AJB_SLICED(beatbox), {0}, GAIN(0.36), {
    [4] = "X.....x.X..x....",      /* mouth kick */
    [5] = "....X.......X..x",      /* "psh" */
    [3] = "..o...o...o...o.",      /* a tick between */
}};

/* Spoons clacking on a knee. */
static const ajb_drum_loop_t spoons = {"Spoons", 16, NULL, {
    [AJB_BD] = "X.......X.......",
    [AJB_RS] = "x.xx..x.x.xx.xx.",
}, GAIN(0.56)};

/* Whisking: a shaker going round with a flick at the end. */
static const ajb_drum_loop_t whisk = {"Whisk", 16, NULL, {
    [AJB_BD] = "X.....x.X.......",
    [AJB_SD] = "....x.......x...",
    [AJB_CP] = "XoxoXoxoXoxoXxxx",
}, GAIN(0.53)};

/* Popcorn: pops scattered all over, a ding when it is done. */
static const ajb_drum_loop_t pop_corn = {"Pop Corn", 32, NULL, {
    [AJB_HT] = "..x....x...x...." "x....x....x..x..",
    [AJB_MT] = "x....x........x." "...x....x......x",
    [AJB_LT] = "....x....x..x..." ".x....x.....x...",
    [AJB_CB] = "X..............." "........x.......",
}, GAIN(1.17)};

/* Stirring: three-step drum circles turning over a steady four. */
static const ajb_drum_loop_t stir_it = {"Stir It", 32, NULL, {
    [AJB_BD] = "X.......X......." "X.......X.......",
    [AJB_RS] = "....x.......x..." "....x.......x...",
    [AJB_LT] = "x..x..x..x..x..x" "..x..x..x..x..x.",
}, GAIN(0.61)};

/* Clanking pan lids. */
static const ajb_drum_loop_t clank = {"Clank", 16, NULL, {
    [AJB_BD] = "X.x.....X.x.....",
    [AJB_SD] = "....X.......X...",
    [AJB_CB] = "x...x..xx...x.x.",
}, GAIN(0.60)};

/* Dishwasher: swishing hats with an accent that drifts round in fives. */
static const ajb_drum_loop_t dish_washer = {"Dish Washer", 32, NULL, {
    [AJB_BD] = "X.......x......." "X.......x.x.....",
    [AJB_SD] = "............x..." "............x...",
    [AJB_CH] = "XxxxxXxxxxXxxxxX" "xxxxXxxxxXxxxxXx",
}, GAIN(0.56)};

/* The first-level tune. */
static const ajb_melody_loop_t level_one = {.name = "Level One", .steps = 32,
    .instrument = &game_lead, .octave = 0, .velocity = 100,
    .riff = "5.5.7.5.8.7.5.3."
            "4.4.5.4.7.5.4.2."};

/* Coins: quick, very high chimes. */
static const ajb_melody_loop_t coin_grab = {.name = "Coin Grab", .steps = 32,
    .instrument = &chip_star, .octave = 2, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 1, .octaves = 1, .chord = {"0235", "1346"}};

/* Low and stomping. */
static const ajb_melody_loop_t boss_fight = {.name = "Boss Fight", .steps = 16,
    .instrument = &rubber_lead, .octave = -1, .velocity = 110,
    .riff = "0.0.0.3.0.0.4.3."};

/* A sweep up three octaves. */
static const ajb_melody_loop_t power_up = {.name = "Power Up", .steps = 16,
    .instrument = &game_lead, .octave = -1, .velocity = 92,
    .dir = AJB_ARP_UP, .rate = 1, .octaves = 3, .chord = {"0235"}};

/* Hopping bloops in threes. */
static const ajb_melody_loop_t pixel_hop = {.name = "Pixel Hop", .steps = 32,
    .instrument = &bloop, .octave = 1, .velocity = 100,
    .riff = "0..2..3.5..3..2."
            "0..2..3.7..5..3."};

/* Gritty chip notes tumbling down. */
static const ajb_melody_loop_t crunch_time = {.name = "Crunch Time", .steps = 32,
    .instrument = &crunchy_chip, .octave = 0, .velocity = 92,
    .dir = AJB_ARP_DOWN, .rate = 2, .octaves = 2, .chord = {"024", "013"}};

/* Scrambled chip notes, new chord each bar. */
static const ajb_melody_loop_t warp_zone = {.name = "Warp Zone", .steps = 64,
    .instrument = &chip_star, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_RANDOM, .rate = 1, .octaves = 2, .chord = {"023", "134", "245", "134"}};

/* The victory tune. */
static const ajb_melody_loop_t high_score = {.name = "High Score", .steps = 64,
    .instrument = &game_lead, .octave = 0, .velocity = 92,
    .riff = "3.3.5---3.3.5---"
            "7.6.5.4.5-------"
            "3.3.5---5.5.7---"
            "9.8.7.6.5-------"};
