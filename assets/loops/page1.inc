/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 1: Bouncy Pop drums, Twinkle melodies. */

/* Four on the floor: kick every beat, open hats on the off-beats. */
static const ajb_drum_loop_t bounce = {"Bounce", 16, NULL, {
    [AJB_BD] = "X...X...X...X...",
    [AJB_CP] = "....X.......X...",
    [AJB_CH] = "o...o...o...o...",
    [AJB_OH] = "..x...x...x...x.",
}, GAIN(0.42)};

/* Half time: one big snare and clap per bar, a crash to start, room to breathe. */
static const ajb_drum_loop_t stomp = {"Stomp", 32, NULL, {
    [AJB_BD] = "X.......x.x.....X.........x.....",
    [AJB_SD] = "........X...............X.......",
    [AJB_CP] = "........X...............X..o....",
    [AJB_CH] = "x...x...x...x...x...x...x...x...",
    [AJB_CY] = "X...............................",
}, GAIN(0.43)};

/* Breakbeat: syncopated kick, ghost snares, busy accented 16th hats. */
static const ajb_drum_loop_t skippy = {"Skippy", 16, NULL, {
    [AJB_BD] = "X.....X...X..X..",
    [AJB_SD] = "....X..o.o..X..o",
    [AJB_CH] = "XoxoXoxoXoxoXoxo",
}, GAIN(0.68)};

/* Cowbell party: 3-3-2 kick, rim clave, cowbell, tom roll into bar 1. */
static const ajb_drum_loop_t cowbell_party = {"Cowbell Party", 32, NULL, {
    [AJB_BD] = "X..x..x.X..x..x.X..x..x.X.......",
    [AJB_RS] = "x..x..x...x.x...x..x..x...x.x...",
    [AJB_CB] = "..x...x...x...x...x...x.........",
    [AJB_HT] = "........................xx......",
    [AJB_MT] = "..........................xx....",
    [AJB_LT] = "............................xxX.",
}, GAIN(0.53)};

/* Clap along: claps on 2 and 4 with a double at the end, straight 8th hats. */
static const ajb_drum_loop_t clap_along = {"Clap Along", 16, NULL, {
    [AJB_BD] = "X.....x.X.......",
    [AJB_CP] = "....X.......X.X.",
    [AJB_CH] = "x.x.x.x.x.x.x.x.",
}, GAIN(0.55)};

/* Shuffle: long-short skipping hats. */
static const ajb_drum_loop_t hop_scotch = {"Hop Scotch", 32, NULL, {
    [AJB_BD] = "X.....x.X..x...." "X.....x.X.x.....",
    [AJB_SD] = "....X.......X..." "....X.......X..o",
    [AJB_CH] = "x..xx..xx..xx..x" "x..xx..xx..xx..x",
}, GAIN(0.72)};

/* Marching band: rolling snare, kick on 1 and 3, a crash to start. */
static const ajb_drum_loop_t big_parade = {"Big Parade", 32, NULL, {
    [AJB_BD] = "X.......X......." "X.......X...X...",
    [AJB_SD] = "x.oox.oox.ooxxxx" "x.oox.ooX.X.X...",
    [AJB_CY] = "X..............." "................",
}, GAIN(0.55)};

/* Clockwork: rim ticks, almost nothing else. */
static const ajb_drum_loop_t tick_tock = {"Tick Tock", 16, NULL, {
    [AJB_BD] = "X.........x.....",
    [AJB_RS] = "x...x...x...x.x.",
    [AJB_CH] = "..o...o...o...o.",
}, GAIN(0.53)};

/* Fast and high: 16th-note chiptune climbing two octaves, new chord each bar. */
static const ajb_melody_loop_t star_steps = {.name = "Star Steps", .steps = 64,
    .instrument = &chip_star, .octave = 1, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 1, .octaves = 2, .chord = {"023", "134", "023", "124"}};

/* The tune: 8th notes with held ends, middle register. */
static const ajb_melody_loop_t music_box_song = {.name = "Music Box Song", .steps = 64,
    .instrument = &toy_piano, .octave = 1, .velocity = 100,
    .riff = "3.5.4.3.2.3.0---"
            "2.3.4.5.4.3.2---"
            "5.7.6.5.4.5.3---"
            "4.3.2.1.0.2.0---"};

/* Low and skipping: a 3-3-2 rhythm on the harp. */
static const ajb_melody_loop_t harp_hop = {.name = "Harp Hop", .steps = 32,
    .instrument = &harp, .octave = 0, .velocity = 110,
    .riff = "0..3..5.4..3..2."
            "0..3..5.7..5..4."};

/* Slow and ringing: big bells falling one beat at a time. */
static const ajb_melody_loop_t bell_tower = {.name = "Bell Tower", .steps = 64,
    .instrument = &tub_bells, .octave = 1, .velocity = 100,
    .dir = AJB_ARP_DOWN, .rate = 4, .octaves = 1, .chord = {"0235", "1346", "0235", "2479"}};

/* Glass raindrops landing anywhere across two octaves. */
static const ajb_melody_loop_t glass_rain = {.name = "Glass Rain", .steps = 64,
    .instrument = &glass_bell, .octave = 1, .velocity = 72,
    .dir = AJB_ARP_RANDOM, .rate = 2, .octaves = 2, .chord = {"023", "024", "134", "023"}};

/* A music box winding up and back down. */
static const ajb_melody_loop_t wind_up = {.name = "Wind-Up", .steps = 32,
    .instrument = &glass_bell, .octave = 0, .velocity = 76,
    .dir = AJB_ARP_BOUNCE, .rate = 2, .octaves = 2, .chord = {"023", "124"}};

/* Careful little steps on the toy piano. */
static const ajb_melody_loop_t tiptoe = {.name = "Tiptoe", .steps = 32,
    .instrument = &toy_piano, .octave = 1, .velocity = 100,
    .riff = "5...3...4.4.2..."
            "3...1...2.2.0..."};

/* Very slow: one long bell note at a time. */
static const ajb_melody_loop_t lullabell = {.name = "Lullabell", .steps = 64,
    .instrument = &glass_bell, .octave = 1, .velocity = 100,
    .riff = "2-------3-------"
            "5-------4---3---"
            "2-------1-------"
            "0---------------"};
