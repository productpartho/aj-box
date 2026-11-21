/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Page 4 in this folder, page 1 on the box: Around the World drums (dhol, tabla,
 * samba, reggaeton) and three drum loops for Zelda tunes; seven tunes from Zelda:
 * Ocarina of Time and the Choir. */

/* The sampled dhol loop, one slice per 16th. */
static const ajb_drum_loop_t dhol_party = {"Dhol Party", 16, AJB_SLICED(dhol), {0}, GAIN(0.37), {
    [0] = "X..x..X...x.X...",      /* dhol bass side */
    [1] = "..x.x..xx.x..xxx",      /* dhol treble side */
}};

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

/* Gerudo Valley drums: the hand claps of the tune, every 16th with the accents
 * of a MIDI transcription by John Kuzma (assets/reference/gerudo-valley-kuzma.mid,
 * the percussion track), and a kick where its bass plays. */
static const ajb_drum_loop_t gerudo_drums = {"Gerudo Drums", 16, NULL, {
    [AJB_BD] = "X...x...X...x...",
    [AJB_CP] = "XooXXoXoXoxXXoXo",
}, GAIN(0.55)};

/* Goron City drums, from the same score (bars 5 and 6): two toms answering,
 * congas on the one and the "and" of two, and the cuica on the sampler's two
 * cuica pads (high and low). */
static const ajb_drum_loop_t goron_drums = {"Goron Drums", 32, NULL, {
    [AJB_MT] = "X.....X........." "X.....X.........",
    [AJB_HT] = "....X.....X....." "....X.....X.....",
    [AJB_LT] = "............X.X." "............X.X.",
}, GAIN(1.99), {
    [6] = "..x...x........." "..x...x.........",
    [7] = "x.......x.x....." "....x...x.x.....",
}};

/* Kokiri Forest drums. The tune has no drums of its own; this follows its
 * plucked strings (same file as the melody below): a low note on beats 1 and 3,
 * light taps on the 8ths between. */
static const ajb_drum_loop_t kokiri_drums = {"Kokiri Drums", 16, NULL, {
    [AJB_BD] = "X.......X.......",
    [AJB_RS] = "..x.x.x...x.x.x.",
    [AJB_CH] = "o...o...o...o...",
}, GAIN(0.6)};

/* Tunes from Zelda: Ocarina of Time, at Partho's request for home use. Written
 * by ear unless a file is named; they use fixed notes, so Mood leaves them alone. Song of Storms and
 * Zelda's Lullaby are waltzes: 48 steps, three beats to the bar (Storms with
 * one step per 8th note, the Lullaby one per 16th), over the 4/4 drums. */

/* Saria's Song. Notes: D4 E4 F4 G4 A4 B4 C5 D5 E5. */
static const int8_t saria_notes[] = {2, 4, 5, 7, 9, 11, 12, 14, 16};
static const ajb_melody_loop_t saria_song = {.name = "Saria's Song", .steps = 64,
    .instrument = &bird_flute, .octave = 0, .velocity = 70,
    .notes = saria_notes, .note_count = 9,
    .riff = "2.4.5---2.4.5---"
            "2.4.5.8.7---5.6."
            "5.3.1-------0.1."
            "3.1---------...."};

/* Song of Storms. Notes: D4 E4 F4 G4 A4 C5 D5 E5 F5. */
static const int8_t storms_notes[] = {2, 4, 5, 7, 9, 12, 14, 16, 17};
static const ajb_melody_loop_t song_of_storms = {.name = "Song of Storms", .steps = 48,
    .instrument = &bird_flute, .octave = 0, .velocity = 70,
    .notes = storms_notes, .note_count = 9,
    .riff = "026---" "026---"
            "7--878" "754---"
            "4-0-23" "4-----"
            "4-0-23" "1-----"};

/* Gerudo Valley: the trumpet tune, bars 8 to 11 of a MIDI transcription by John
 * Kuzma (assets/reference/gerudo-valley-kuzma.mid, 4/4, written at 120 BPM).
 * Notes: B3 C#4 D4 E4 F4 F#4 G#4 A4. */
static const int8_t gerudo_notes[] = {-1, 1, 2, 4, 5, 6, 8, 9};
static const ajb_melody_loop_t gerudo_valley = {.name = "Gerudo Valley", .steps = 64,
    .instrument = &candy_brass, .octave = 0, .velocity = 100,
    .notes = gerudo_notes, .note_count = 8,
    .riff = ".1567-.1567----."
            ".2567-.2567----."
            ".0356-.0356----."
            ".5654----------."};

/* A choir climbing one beat at a time. */
static const ajb_melody_loop_t choir_rise = {.name = "Choir", .steps = 64,
    .instrument = &choir, .octave = 0, .velocity = 100,
    .dir = AJB_ARP_UP, .rate = 4, .octaves = 1, .chord = {"023", "134", "245", "134"}};

/* Epona's Song, by ear: a waltz like Song of Storms, 48 steps with one step per
 * 8th note, eight bars of three beats. Notes: A4 B4 D5. */
static const int8_t epona_notes[] = {9, 11, 14};
static const ajb_melody_loop_t eponas_song = {.name = "Epona's Song", .steps = 48,
    .instrument = &bird_flute, .octave = 0, .velocity = 100,
    .notes = epona_notes, .note_count = 3,
    .riff = "2-1-0-" "------"
            "2-1-0-" "------"
            "2-1-0-" "1-----"
            "0-----" "----.."};

/* Zelda's Lullaby. Notes: G4 A4 B4 C5 D5 G5 A5. */
static const int8_t lullaby_notes[] = {7, 9, 11, 12, 14, 19, 21};
static const ajb_melody_loop_t zeldas_lullaby = {.name = "Zelda's Lullaby", .steps = 48,
    .instrument = &bird_flute, .octave = 0, .velocity = 100,
    .notes = lullaby_notes, .note_count = 7,
    .riff = "2-------4---"
            "1-------0-1-"
            "2-------4---"
            "1-----------"};

/* Goron City: the marimba riff, bars 9 to 12 of the score (4/4, written at
 * 115 BPM). Notes: G3 A#3 B3 C4 E4. From a MIDI file of a transcription by
 * Eric Christensen (assets/reference). */
static const int8_t goron_notes[] = {-5, -2, -1, 0, 4};
static const ajb_melody_loop_t goron_city = {.name = "Goron City", .steps = 64,
    .instrument = &toy_guitar, .octave = 0, .velocity = 110,
    .notes = goron_notes, .note_count = 5,
    .riff = "4-30..0-1-3-...."
            "4-30..3-1-3-...2"
            "4-30..0-1-3-...."
            "4-303-0-1-3-...."};

/* Kokiri Forest: the ocarina tune and the harp's answer, bars 10 to 13 of a MIDI
 * transcription by John Kuzma (assets/reference/kokiri-forest-kuzma.mid, 4/4,
 * written at 145 BPM). Notes: C4 D4 E4 F4 G4 A4 A#4 C5 D5. */
static const int8_t kokiri_notes[] = {0, 2, 4, 5, 7, 9, 10, 12, 14};
static const ajb_melody_loop_t kokiri_forest = {.name = "Kokiri Forest", .steps = 64,
    .instrument = &bird_flute, .octave = 0, .velocity = 100,
    .notes = kokiri_notes, .note_count = 9,
    .riff = "4-7.4.2.1--2--3-"
            "454---343---2.1."
            "2.0-----6.3.6.8."
            "7-----4---3.1.3."};
