/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 4: Around the World drums (dhol, tabla, samba, reggaeton), Animal
 * Choir melodies (cat, bird, voices). */

/* The sampled dhol loop, one slice per 16th. */
static const ajb_drum_loop_t dhol_party = {"Dhol Party", 16, &ajb_sample_dhol, {0}, GAIN(0.37)};

/* Reggaeton: four on the floor under the dembow snare, a shaker on top. */
static const ajb_drum_loop_t reggaeton = {"Reggaeton", 16, NULL, {
    [AJB_BD] = "X...X...X...X...",
    [AJB_SD] = "...x..x....x..x.",
    [AJB_CP] = "o.o.o.o.o.o.o.o.",
}, GAIN(0.62)};

/* Samba: a swinging low drum, rim pattern, shaker 16ths, cowbell agogo. */
static const ajb_drum_loop_t samba = {"Samba", 32, NULL, {
    [AJB_BD] = "X..xX..xX..xX..x" "X..xX..xX..xX..x",
    [AJB_RS] = "x..x..x...x.x..." "x..x..x...x..x..",
    [AJB_CP] = "xooxxooxxooxxoox" "xooxxooxxooxxoox",
    [AJB_CB] = "x...x.x...x.x..." "x...x.x...x.x...",
}, GAIN(0.49)};

/* Tabla talk: three hand drums in conversation, no kick. */
static const ajb_drum_loop_t tabla_talk = {"Tabla Talk", 32, NULL, {
    [AJB_HT] = "x.xx..x.x.xx..x." "x.xx..x.xxxx..x.",
    [AJB_MT] = "....x.......x..." "....x.......x.x.",
    [AJB_LT] = "X.....x.X..x...." "X.....x.X.......",
}, GAIN(1.50)};

/* Afrobeat: a three-step cowbell rolling across the four-beat bar. */
static const ajb_drum_loop_t afrobeat = {"Afrobeat", 32, NULL, {
    [AJB_BD] = "X.....x...X....." "X.....x...X..x..",
    [AJB_SD] = "....X.......X..." "....X.......X...",
    [AJB_CB] = "x..x..x..x..x..." "x..x..x..x..x.x.",
    [AJB_CH] = "..x...x...x...x." "..x...x...x...x.",
}, GAIN(0.61)};

/* Son clave on the rim, shaker, a kick pushing into beat 3. */
static const ajb_drum_loop_t clave_walk = {"Clave Walk", 16, NULL, {
    [AJB_BD] = "X......xX.......",
    [AJB_RS] = "x..x..x...x.x...",
    [AJB_CP] = "o.o.o.o.o.o.o.o.",
}, GAIN(0.54)};

/* Carnival: marching kick, off-beat snare with rolls, bell, a crash to start. */
static const ajb_drum_loop_t carnival = {"Carnival", 32, NULL, {
    [AJB_BD] = "X...X...X...X..." "X...X...X...X...",
    [AJB_SD] = "..x...x...x..xx." "..x...x...xxxxx.",
    [AJB_CB] = "x.x..x.x.x..x.x." "x.x..x.x.x..x.x.",
    [AJB_CY] = "X..............." "................",
}, GAIN(0.52)};

/* Island: 3-3-2 kick, rim on 2 and 4, two hand drums. */
static const ajb_drum_loop_t island = {"Island", 16, NULL, {
    [AJB_BD] = "X..x..x.X..x..x.",
    [AJB_RS] = "....x.......x...",
    [AJB_HT] = "..x......x......",
    [AJB_MT] = "......x.......x.",
}, GAIN(0.76)};

/* The cat sings a tune. */
static const ajb_melody_loop_t cat_song = {.name = "Cat Song", .steps = 32,
    .instrument = &cat, .octave = 0, .velocity = 110,
    .riff = "5---3---4---2---"
            "5---3---1-2-0---"};

/* Her own voice: la la laaa. */
static const ajb_melody_loop_t la_la_la = {.name = "La La La", .steps = 32,
    .instrument = &her_la, .octave = 1, .velocity = 110,
    .riff = "0.2.3---3.2.0---"
            "2.3.5---4.3.2---"};

/* Bird calls, high, with gaps to answer in. */
static const ajb_melody_loop_t bird_call = {.name = "Bird Call", .steps = 32,
    .instrument = &bird_flute, .octave = 0, .velocity = 127,
    .riff = "7.9.....7.9....."
            "5.7.9.7.5......."};

/* A choir climbing one beat at a time. */
static const ajb_melody_loop_t choir_rise = {.name = "Choir", .steps = 64,
    .instrument = &choir, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 4, .octaves = 1, .chord = {"023", "134", "245", "134"}};

/* A low frog: ribbit, ribbit. */
static const ajb_melody_loop_t frog_hop = {.name = "Frog Hop", .steps = 16,
    .instrument = &bloop, .octave = -1, .velocity = 100,
    .riff = "0..0....3..0...."};

/* Kittens running up two octaves. */
static const ajb_melody_loop_t kitten_chase = {.name = "Kitten Chase", .steps = 32,
    .instrument = &cat, .octave = 0, .velocity = 110,
    .dir = AJB_ARP_UP, .rate = 2, .octaves = 2, .chord = {"023", "134"}};

/* Her voice singing chords up and down. */
static const ajb_melody_loop_t sing_along = {.name = "Sing Along", .steps = 64,
    .instrument = &her_la, .octave = 0, .velocity = 110,
    .dir = AJB_ARP_BOUNCE, .rate = 2, .octaves = 1, .chord = {"0235", "0245", "1346", "0235"}};

/* An owl: hoo-hoooo, then quiet. */
static const ajb_melody_loop_t owl_night = {.name = "Owl Night", .steps = 64,
    .instrument = &bird_flute, .octave = 0, .velocity = 68,
    .riff = "3-..3---........"
            "2-..2---........"
            "3-..3---........"
            "0-..0-------...."};
