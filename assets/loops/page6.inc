/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 6: Sleepy Time drums (brushes, rain stick), Dreamy melodies (kalimba,
 * harp, toy piano). */

/* The sampled rain stick, one slice per 16th. */
static const ajb_drum_loop_t rain_stick = {"Rain Stick", 16, AJB_SLICED(rainstick), {0}, GAIN(0.36), {
    [5] = "oxooxoooxoooxoox",      /* rain grains */
}};

/* Brushes: a soft swishing snare, hardly any kick. */
static const ajb_drum_loop_t brushes = {"Brushes", 32, NULL, {
    [AJB_BD] = "X.........x....." "X...............",
    [AJB_SD] = "o.o.x.o.o.o.x.o." "o.o.x.o.o.o.x.oo",
}, GAIN(0.68)};

/* A heartbeat: lub-dub, lub-dub. */
static const ajb_drum_loop_t heartbeat = {"Heartbeat", 16, NULL, {
    [AJB_BD] = "X..x....X..x....",
    [AJB_RS] = "............o...",
}, GAIN(0.73)};

/* A rocking chair: creak forward, creak back. */
static const ajb_drum_loop_t rocking_chair = {"Rocking Chair", 32, NULL, {
    [AJB_BD] = "X.......x......." "X.......x...x...",
    [AJB_RS] = "x.....o.x.....o." "x.....o.x.....o.",
    [AJB_CP] = "....o.......o..." "....o.......o...",
}, GAIN(0.62)};

/* Tiptoeing hats, one kick, one rim. */
static const ajb_drum_loop_t tippy_toes = {"Tippy Toes", 16, NULL, {
    [AJB_BD] = "X...............",
    [AJB_RS] = "........x.......",
    [AJB_CH] = "o..o..o.o..o..o.",
}, GAIN(0.67)};

/* Soft shoe shuffle. */
static const ajb_drum_loop_t soft_shoe = {"Soft Shoe", 32, NULL, {
    [AJB_BD] = "X.......x......." "X.......x.......",
    [AJB_SD] = "....o.......o..." "....o.......o.o.",
    [AJB_CH] = "x..ox..ox..ox..o" "x..ox..ox..ox..o",
}, GAIN(0.63)};

/* Crickets chirping in pairs. */
static const ajb_drum_loop_t crickets = {"Crickets", 16, NULL, {
    [AJB_BD] = "x...............",
    [AJB_RS] = "......o.......o.",
    [AJB_CH] = "xx..xx..xx..xx..",
}, GAIN(0.78)};

/* Four slow bars with almost nothing in them. */
static const ajb_drum_loop_t slow_snow = {"Slow Snow", 64, NULL, {
    [AJB_BD] = "X..............." "................" "x..............." "................",
    [AJB_RS] = "................" "........o......." "................" "........o...o...",
    [AJB_CY] = "X..............." "................" "................" "................",
    [AJB_CH] = "....o.......o..." "....o.......o..." "....o.......o..." "....o.......o...",
}, GAIN(0.63)};

/* A rolling kalimba tune. */
static const ajb_melody_loop_t kalimba_dream = {.name = "Kalimba Dream", .steps = 32,
    .instrument = &toy_guitar, .octave = 1, .velocity = 110,
    .riff = "0.3.5.3.4.3.2.3."
            "0.3.5.3.7.5.4.3."};

/* Harp chords rising across two octaves. */
static const ajb_melody_loop_t harp_glow = {.name = "Harp Glow", .steps = 64,
    .instrument = &harp, .octave = 0, .velocity = 110,
    .dir = AJB_ARP_UP, .rate = 2, .octaves = 2, .chord = {"0235", "2457", "1346", "0235"}};

/* A nursery tune on the toy piano. */
static const ajb_melody_loop_t toy_piano_tune = {.name = "Toy Piano Tune", .steps = 64,
    .instrument = &toy_piano, .octave = 0, .velocity = 100,
    .riff = "5---3-4-5---7---"
            "6---4-5-6---4---"
            "5---3-4-5---8---"
            "7---6---5-------"};

/* Strings swelling, one note every two beats. */
static const ajb_melody_loop_t cloud_ride = {.name = "Cloud Ride", .steps = 64,
    .instrument = &dream_strings, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 8, .octaves = 1, .chord = {"02", "13", "24", "13"}};

/* The music box winding down, one note a beat. */
static const ajb_melody_loop_t sleepy_box = {.name = "Sleepy Box", .steps = 64,
    .instrument = &glass_bell, .octave = 1, .velocity = 96,
    .dir = AJB_ARP_BOUNCE, .rate = 4, .octaves = 1, .chord = {"0235", "0245", "1346", "0235"}};

/* Thumbs wandering over the kalimba. */
static const ajb_melody_loop_t thumb_piano = {.name = "Thumb Piano", .steps = 32,
    .instrument = &harp, .octave = 0, .velocity = 110,
    .dir = AJB_ARP_RANDOM, .rate = 2, .octaves = 2, .chord = {"0234", "1345"}};

/* Two far-apart bells per bar. */
static const ajb_melody_loop_t moonbeam = {.name = "Moonbeam", .steps = 64,
    .instrument = &glass_bell, .octave = 1, .velocity = 96,
    .dir = AJB_ARP_DOWN, .rate = 8, .octaves = 1, .chord = {"35", "24", "35", "02"}};

/* A slow harp tune stepping down to rest. */
static const ajb_melody_loop_t goodnight = {.name = "Goodnight", .steps = 64,
    .instrument = &harp, .octave = 0, .velocity = 115,
    .riff = "5---4---3---2---"
            "3---2---1---0---"
            "2---3---4---2---"
            "0---------------"};
