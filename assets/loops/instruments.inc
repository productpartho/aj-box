/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * The 24 instruments (PRD "Sound swaps", ALGORITHM): 20 synthesized on FM1
 * Open's engines and 4 sampled. Knob numbers are the firmware's own:
 *   PSG    0 MODE (0 YM, 1 AY, 2 noise, 3 mix) 1 NOISE 2 TUNE 3 RELEASE
 *   Pluck  0 DECAY 1 BRIGHT 2 TUNE 3 RELEASE
 *   303    0 Cutoff 1 Reso 2 EnvMod 3 Decay 4 Accent 5 Wave (0 saw, 1 square)
 * FM instruments are DX7 factory voices, by program number. */
#define FM(id, label, voice, level) static const ajb_instrument_t id = {.name = label, \
    .kind = AJB_INSTRUMENT_FM, .program = voice, .gain_q15 = GAIN(level)}
#define SAMPLED(id, label, sound, level) static const ajb_instrument_t id = {.name = label, \
    .kind = AJB_INSTRUMENT_SAMPLE, .sample = &sound, .gain_q15 = GAIN(level)}

/* Chiptune */
static const ajb_instrument_t chip_star = {.name = "Chip Star", .kind = AJB_INSTRUMENT_PSG,
    .gain_q15 = GAIN(0.88), .knobs = {{3, 12}}, .knob_count = 1};
static const ajb_instrument_t bloop = {.name = "Bloop", .kind = AJB_INSTRUMENT_PSG,
    .gain_q15 = GAIN(1.05), .knobs = {{0, 1}, {3, 6}}, .knob_count = 2};
static const ajb_instrument_t game_lead = {.name = "Game Lead", .kind = AJB_INSTRUMENT_PSG,
    .gain_q15 = GAIN(1.07), .knobs = {{3, 40}}, .knob_count = 1};
static const ajb_instrument_t crunchy_chip = {.name = "Crunchy Chip", .kind = AJB_INSTRUMENT_PSG,
    .gain_q15 = GAIN(1.07), .knobs = {{0, 3}, {1, 10}, {3, 16}}, .knob_count = 3};

/* Plucked strings: a soft ringing harp, a bright short toy guitar. */
static const ajb_instrument_t harp = {.name = "Harp", .kind = AJB_INSTRUMENT_PLUCK,
    .gain_q15 = GAIN(1.1), .knobs = {{0, 104}, {1, 64}, {3, 70}}, .knob_count = 3};
static const ajb_instrument_t toy_guitar = {.name = "Toy Guitar", .kind = AJB_INSTRUMENT_PLUCK,
    .gain_q15 = GAIN(0.94), .knobs = {{0, 70}, {1, 120}, {3, 20}}, .knob_count = 3};

/* 303: a round bass, a squelchy acid line, a hollow square lead. */
static const ajb_instrument_t bouncy_bass = {.name = "Bouncy Bass", .kind = AJB_INSTRUMENT_BASS,
    .gain_q15 = GAIN(0.34), .knobs = {{0, 40}, {1, 80}, {2, 70}, {3, 40}, {4, 90}}, .knob_count = 5};
static const ajb_instrument_t squelch = {.name = "Squelch", .kind = AJB_INSTRUMENT_BASS,
    .gain_q15 = GAIN(0.31), .knobs = {{0, 50}, {1, 110}, {2, 100}, {3, 50}, {4, 100}}, .knob_count = 5};
static const ajb_instrument_t rubber_lead = {.name = "Rubber Lead", .kind = AJB_INSTRUMENT_BASS,
    .gain_q15 = GAIN(0.78), .knobs = {{0, 80}, {1, 50}, {2, 60}, {3, 70}, {5, 1}}, .knob_count = 5};

/* DX7 */
FM(tub_bells, "Tub Bells", 120, 0.515);
FM(glass_bell, "Glass Bell", 100, 1.1);
FM(bubble_piano, "Bubble Piano", 0, 0.52);
FM(toy_piano, "Toy Piano", 4, 0.68);
FM(party_organ, "Party Organ", 21, 0.34);
FM(candy_brass, "Candy Brass", 80, 0.7);
FM(laser, "Laser", 2, 0.34);
FM(space_pad, "Space Pad", 3, 0.88);
FM(bird_flute, "Bird Flute", 77, 1.05);
FM(choir, "Choir", 67, 0.55);
FM(dream_strings, "Dream Strings", 70, 0.88);

/* Sampled (PRD "The 8 sampled sounds"); placeholders until the real recordings. */
/* The four sampled instruments (Music Box, Kalimba, La, Cat) were retired on
 * Nov 14, 2026: the stock sampler plays one-shots only. */

/* ALGORITHM order: neighbours come from different engines so each press is a clear change. */
static const ajb_instrument_t *const instruments[] = {
    &chip_star, &harp, &tub_bells, &bubble_piano, &bouncy_bass, &party_organ,
    &bloop, &glass_bell, &laser, &toy_guitar, &candy_brass, &squelch, &space_pad,
    &game_lead, &toy_piano, &bird_flute, &rubber_lead, &choir, &crunchy_chip, &dream_strings,
};
