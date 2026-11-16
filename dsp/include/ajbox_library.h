/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Loop library data types (PRD "Loop library and storage"). */
#ifndef AJB_LIBRARY_H
#define AJB_LIBRARY_H
#include <stdint.h>

enum { AJB_SLOTS = 8 };            /* loop keys per side per page */
enum { AJB_PADS = 8 };             /* one-shot pads on the stock sampler */
enum { AJB_SAMPLE_RATE_IN = 32000 }; /* PRD "Storage math": SEA5 at 32 kHz, the stock format */

/* One recorded sound: a pitched note (slices = 0) or a loop cut into
 * `slices` equal slices, one per 16th step (PRD "The 8 sampled sounds"). */
typedef struct {
    const char *name;
    const uint8_t *data;          /* SEA5 chunks, 228 bytes per 320 frames */
    uint32_t frames;
    uint16_t rate;
    uint8_t root_note;            /* MIDI note of a pitched recording */
    uint8_t slices;
} ajb_sample_t;

/* DRUM 808 parts, same order as firmware D8_BD..D8_CH (checked in engine.c). */
enum { AJB_BD, AJB_SD, AJB_LT, AJB_MT, AJB_HT, AJB_RS, AJB_CP, AJB_CB, AJB_CY,
       AJB_OH, AJB_CH, AJB_PARTS };

/* Step strings, one character per 16th: '.' rest, 'o' soft, 'x' normal,
 * 'X' accent. A sliced loop ignores `parts` and plays one slice per step. */
typedef struct {
    const char *name;
    uint8_t steps;                /* 16, 32 or 64: 1, 2 or 4 bars (a melody may use 48 for a waltz) */
    const ajb_sample_t *sliced;
    const char *parts[AJB_PARTS];
    uint16_t gain_q15;
    /* On the stock instrument: step strings for the sampler's 8 one-shot pads
     * (dsp/stock/oneshots.h), played beside `parts`. The old engine ignores them. */
    const char *pads[AJB_PADS];
} ajb_drum_loop_t;

/* A kit is a list of DRUM 808 parameter edits on top of the engine defaults
 * (part, parameter index as in the firmware descriptors, value). */
typedef struct { uint8_t part, parameter, value; } ajb_kit_edit_t;
typedef struct {
    const char *name;
    const ajb_kit_edit_t *edits;
    unsigned count;
} ajb_kit_t;

/* Melody engines (PRD "Sound swaps"): FM1 Open's DX7, Pluck (plucked
 * string), PSG (AY-3-8910 chiptune) and 303 (mono acid bass), or a sample. */
enum { AJB_INSTRUMENT_FM, AJB_INSTRUMENT_SAMPLE, AJB_INSTRUMENT_PLUCK, AJB_INSTRUMENT_PSG,
       AJB_INSTRUMENT_BASS };
/* Knob settings for Pluck, PSG and 303 (parameter index as in the firmware
 * descriptors: pluck.c, psg.c, bass303.h); unlisted knobs keep defaults. */
typedef struct { uint8_t parameter, value; } ajb_knob_t;
enum { AJB_KNOBS_MAX = 12 };
typedef struct {
    const char *name;
    uint8_t kind;
    uint8_t program;              /* factory DX7 voice 0..127 for FM */
    const ajb_sample_t *sample;   /* for AJB_INSTRUMENT_SAMPLE */
    uint16_t gain_q15;
    ajb_knob_t knobs[AJB_KNOBS_MAX];
    uint8_t knob_count;
} ajb_instrument_t;

enum { AJB_ARP_UP, AJB_ARP_DOWN, AJB_ARP_BOUNCE, AJB_ARP_RANDOM };
enum { AJB_CHORD_MAX = 8, AJB_ARP_BARS = 4 };
/* Melody notes are scale degrees, so Mood can change the scale later:
 * degree d plays scale[d % 5] + 12 * (d / 5) above the mood root.
 * Riff characters: '0'..'9' and 'a'..'f' degrees 0..15, '-' holds the
 * previous note, '.' rest. An arpeggio (riff == NULL) plays the degrees in
 * chord[bar] (same characters; NULL repeats chord[0]) every `rate` steps in
 * direction `dir` across `octaves` octaves. */
typedef struct {
    const char *name;
    uint8_t steps;
    const ajb_instrument_t *instrument;
    int8_t octave;                /* octaves above the mood root (C4) */
    uint8_t velocity;
    const char *riff;
    uint8_t dir, rate, octaves;
    const char *chord[AJB_ARP_BARS];
    /* A tune that needs notes outside the 5-note scale: degree d plays
     * notes[d] semitones above C4 (plus octaves), whatever the Mood. */
    const int8_t *notes;
    uint8_t note_count;
} ajb_melody_loop_t;

/* A page: 8 drum and 8 melody loops (PRD "Loop library and storage").
 * `kit` is the page's own drum kit, used when she picks a drum loop from it
 * until PRESETS chooses another. */
typedef struct {
    const char *name;
    const ajb_drum_loop_t *drums[AJB_SLOTS];
    const ajb_melody_loop_t *melody[AJB_SLOTS];
    uint8_t kit;
} ajb_page_t;

/* PRESETS steps through `kits`, ALGORITHM through `instruments` (PRD "Sound
 * swaps"). `fill` is what the drum Fill key plays while held. */
typedef struct {
    const ajb_page_t *pages;
    unsigned page_count;
    const ajb_kit_t *const *kits;
    unsigned kit_count;
    const ajb_instrument_t *const *instruments;
    unsigned instrument_count;
    const ajb_drum_loop_t *fill;
    const ajb_sample_t *const *samples;
    unsigned sample_count;
} ajb_library_t;

extern const ajb_library_t ajb_factory_library;
#endif
