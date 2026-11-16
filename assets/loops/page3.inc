/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 3: Head-Nod drums (hip-hop, lo-fi), Space melodies (lasers, bloops, pads). */

/* Boom bap: kick, snare, kick-kick, snare. */
static const ajb_drum_loop_t boom_bap = {"Boom Bap", 16, NULL, {
    [AJB_BD] = "X......x..X.....",
    [AJB_SD] = "....X.......X...",
    [AJB_CH] = "x.x.x.x.x.x.x.x.",
}, GAIN(0.68)};

/* Laid back: sparse quarter-note hats, a late kick. */
static const ajb_drum_loop_t lazy_sunday = {"Lazy Sunday", 32, NULL, {
    [AJB_BD] = "X.........x....." "X......x..x.....",
    [AJB_SD] = "....X.......X..." "....X.......X..o",
    [AJB_CH] = "x...x...x...x..." "x...x...x...x...",
    [AJB_OH] = "..............x." "................",
}, GAIN(0.68)};

/* Double kick up front, galloping hats, claps. */
static const ajb_drum_loop_t head_bop = {"Head Bop", 16, NULL, {
    [AJB_BD] = "X..X......X.....",
    [AJB_CP] = "....X.......X...",
    [AJB_CH] = "x.xxx.xxx.xxx.xx",
}, GAIN(0.68)};

/* Half-time trap: one clap a bar, hat rolls. */
static const ajb_drum_loop_t trap_cat = {"Trap Cat", 32, NULL, {
    [AJB_BD] = "X.........X....." "X......X........",
    [AJB_CP] = "........X......." "........X.......",
    [AJB_CH] = "x.x.x.x.x.x.xxxx" "x.x.xxx.x.x.x.x.",
}, GAIN(0.68)};

/* Dusty: soft-loud hats and a wandering rim. */
static const ajb_drum_loop_t dusty = {"Dusty", 16, NULL, {
    [AJB_BD] = "X.....X..x......",
    [AJB_SD] = "....X.......X...",
    [AJB_RS] = "..o....o..o...o.",
    [AJB_CH] = "o.x.o.x.o.x.o.x.",
}, GAIN(0.75)};

/* Slow jam: half time, the snare lands once a bar. */
static const ajb_drum_loop_t slow_jam = {"Slow Jam", 32, NULL, {
    [AJB_BD] = "X..........x...." "X.........x.....",
    [AJB_SD] = "........X......." "........X.....o.",
    [AJB_CH] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x.x.",
}, GAIN(0.75)};

/* Stop-start kick, ghost claps, hats with gaps. */
static const ajb_drum_loop_t scratch = {"Scratch", 16, NULL, {
    [AJB_BD] = "X..x..X.....x...",
    [AJB_SD] = "....X.......X...",
    [AJB_CP] = ".......o.o......",
    [AJB_CH] = "x...x.x.x...x.x.",
    [AJB_OH] = "..x.......x.....",
}, GAIN(0.55)};

/* Mostly hats: a tapping pattern with a rim backbeat. */
static const ajb_drum_loop_t tip_tap = {"Tip Tap", 16, NULL, {
    [AJB_BD] = "X.........X.....",
    [AJB_RS] = "....x.......x...",
    [AJB_CH] = "x.oxx.oxx.oxx.ox",
}, GAIN(0.81)};

/* Lasers falling fast. */
static const ajb_melody_loop_t laser_tag = {.name = "Laser Tag", .steps = 32,
    .instrument = &laser, .octave = 1, .velocity = 90,
    .dir = AJB_ARP_DOWN, .rate = 1, .octaves = 2, .chord = {"024", "135"}};

/* Slow pad chords, one note every two beats. */
static const ajb_melody_loop_t moon_walk = {.name = "Moon Walk", .steps = 64,
    .instrument = &space_pad, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 8, .octaves = 1, .chord = {"0235", "1346", "2457", "1346"}};

/* Two bloops calling to each other. */
static const ajb_melody_loop_t bloop_bloop = {.name = "Bloop Bloop", .steps = 16,
    .instrument = &bloop, .octave = 1, .velocity = 100,
    .riff = "0..5..0.3..7..3."};

/* Count up, then float back down. */
static const ajb_melody_loop_t rocket = {.name = "Rocket", .steps = 32,
    .instrument = &rubber_lead, .octave = 0, .velocity = 110,
    .riff = "0.1.2.3.4.5.6.7."
            "8---7---5---3---"};

/* A robot stuttering on two notes. */
static const ajb_melody_loop_t robot_talk = {.name = "Robot Talk", .steps = 16,
    .instrument = &crunchy_chip, .octave = 0, .velocity = 100,
    .riff = "0.00.0..3.33.3.."};

/* A computer thinking: random bloops over three octaves. */
static const ajb_melody_loop_t computer_brain = {.name = "Computer Brain", .steps = 16,
    .instrument = &bloop, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_RANDOM, .rate = 1, .octaves = 3, .chord = {"01234"}};

/* Whole-bar pad notes drifting by. */
static const ajb_melody_loop_t star_drift = {.name = "Star Drift", .steps = 64,
    .instrument = &space_pad, .octave = 1, .velocity = 100,
    .riff = "5---------------"
            "4---------------"
            "7---------------"
            "6-------4-------"};

/* A saucer wobbling up and down the whole scale. */
static const ajb_melody_loop_t ufo = {.name = "UFO", .steps = 16,
    .instrument = &laser, .octave = 0, .velocity = 90,
    .dir = AJB_ARP_BOUNCE, .rate = 1, .octaves = 1, .chord = {"01234"}};
