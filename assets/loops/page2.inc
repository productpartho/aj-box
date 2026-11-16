/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 2: Disco Dance drums, Bubblegum melodies (bright synth chords). */

/* Classic disco: four on the floor, snare on 2 and 4, "pea-soup" open hats. */
static const ajb_drum_loop_t glitter_ball = {"Glitter Ball", 16, NULL, {
    [AJB_BD] = "X...X...X...X...",
    [AJB_SD] = "....X.......X...",
    [AJB_CH] = "xo.oxo.oxo.oxo.o",
    [AJB_OH] = "..x...x...x...x.",
}, GAIN(0.50)};

/* Rolling 3-3-2 kick under straight hats and claps. */
static const ajb_drum_loop_t roller_skate = {"Roller Skate", 32, NULL, {
    [AJB_BD] = "X..x..x.X..x...." "X..x..x.X.x.x...",
    [AJB_CP] = "....X.......X..." "....X.......X...",
    [AJB_CH] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x.x.",
    [AJB_OH] = "..............x." "..............x.",
}, GAIN(0.49)};

/* Funk: kick and ghost snares dodge each other, cowbell pecks. */
static const ajb_drum_loop_t funky_chicken = {"Funky Chicken", 16, NULL, {
    [AJB_BD] = "X..x......x..x..",
    [AJB_SD] = "....X..o.o..X...",
    [AJB_CB] = "..o..o....o..o..",
    [AJB_CH] = "x.x.x.x.x.x.x.x.",
}, GAIN(0.67)};

/* All about the claps. */
static const ajb_drum_loop_t clap_hustle = {"Hand Clap Hustle", 16, NULL, {
    [AJB_BD] = "X...X...X...X...",
    [AJB_CP] = "....X..x....X.xx",
    [AJB_CH] = "..x...x...x...x.",
}, GAIN(0.42)};

/* Toms answer each other high to low. */
static const ajb_drum_loop_t tom_tom_club = {"Tom Tom Club", 32, NULL, {
    [AJB_BD] = "X.......X......." "X.......X.......",
    [AJB_CP] = "....x.......x..." "....x.......x...",
    [AJB_HT] = "..xx............" "..x..x..........",
    [AJB_MT] = "......xx........" "........x..x....",
    [AJB_LT] = "..........xx..x." "............xxX.",
}, GAIN(0.55)};

/* Shaking 16th hats with the accent off the beat. */
static const ajb_drum_loop_t shimmy = {"Shimmy", 16, NULL, {
    [AJB_BD] = "X..x....X.x.....",
    [AJB_SD] = "....X.......X...",
    [AJB_CH] = "xxXxxxXxxxXxxxXx",
}, GAIN(0.68)};

/* Four-bar build: hats double each bar, then a snare roll back to the top. */
static const ajb_drum_loop_t mirror_ball = {"Mirror Ball", 64, NULL, {
    [AJB_BD] = "X...X...X...X..." "X...X...X...X..." "X...X...X...X..." "X...X...X...X...",
    [AJB_SD] = "................" "....x.......x..." "....x.......x..." "....x...x.x.XXXX",
    [AJB_CP] = "................" "................" "....X.......X..." "....X...........",
    [AJB_CH] = "..x...x...x...x." "x.x.x.x.x.x.x.x." "xxxxxxxxxxxxxxxx" "xxxxxxxxxxxx....",
    [AJB_CY] = "X..............." "................" "................" "................",
}, GAIN(0.38)};

/* Cash register: a galloping cowbell. */
static const ajb_drum_loop_t cha_ching = {"Cha-Ching", 16, NULL, {
    [AJB_BD] = "X.....X.X.......",
    [AJB_SD] = "....X.......X...",
    [AJB_CB] = "x.xx.xx.x.xx.xx.",
    [AJB_OH] = "..............x.",
}, GAIN(0.66)};

/* Organ chords bubbling upward. */
static const ajb_melody_loop_t bubble_pop = {.name = "Bubble Pop", .steps = 64,
    .instrument = &party_organ, .octave = 1, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 2, .octaves = 1, .chord = {"0235", "0235", "2457", "1346"}};

/* A bouncy piano tune, up and over. */
static const ajb_melody_loop_t gumdrop = {.name = "Gumdrop", .steps = 32,
    .instrument = &bubble_piano, .octave = 0, .velocity = 100,
    .riff = "0.2.3.2.5.3.2.3."
            "1.3.4.3.6.4.3.1."};

/* Octave-jumping disco bass. */
static const ajb_melody_loop_t disco_strut = {.name = "Disco Strut", .steps = 16,
    .instrument = &bouncy_bass, .octave = -1, .velocity = 110,
    .riff = "0.5.0.5.0.5.3.4."};

/* Brass stabs. */
static const ajb_melody_loop_t fizzy_pop = {.name = "Fizzy Pop", .steps = 32,
    .instrument = &candy_brass, .octave = 0, .velocity = 110,
    .riff = "5..5..5.....4.5."
            "7..7..7.....5.4."};

/* Racing acid squelch. */
static const ajb_melody_loop_t sugar_rush = {.name = "Sugar Rush", .steps = 64,
    .instrument = &squelch, .octave = -1, .velocity = 110,
    .dir = AJB_ARP_UP, .rate = 1, .octaves = 2, .chord = {"03", "03", "14", "24"}};

/* Fairground organ tune. */
static const ajb_melody_loop_t roller_rink = {.name = "Roller Rink", .steps = 64,
    .instrument = &party_organ, .octave = 0, .velocity = 100,
    .riff = "3-4-5---3-4-5---"
            "7-6-5-4-3-------"
            "3-4-5---5-6-7---"
            "8-7-6-5-5-------"};

/* Piano running up and down the stairs. */
static const ajb_melody_loop_t sparkle_stairs = {.name = "Sparkle Stairs", .steps = 64,
    .instrument = &bubble_piano, .octave = 1, .velocity = 62,
    .dir = AJB_ARP_BOUNCE, .rate = 1, .octaves = 2, .chord = {"023", "245", "134", "023"}};

/* Guitar tumbling down. */
static const ajb_melody_loop_t jelly_wobble = {.name = "Jelly Wobble", .steps = 32,
    .instrument = &toy_guitar, .octave = 0, .velocity = 110,
    .dir = AJB_ARP_DOWN, .rate = 2, .octaves = 2, .chord = {"023", "134"}};
