/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's app logic on FM1 Open's stock instrument (docs/s2-design.md):
 * what each key, button and knob in PRD "Control map" does, as calls on the
 * stock instrument (firmware/app/instrument.h). The same file is meant to
 * run on the FM-1 and in the desktop simulator. Control side only, except
 * ajapp_audio, which the audio output stage calls once per block. */
#ifndef AJAPP_H
#define AJAPP_H
#include <stdint.h>
#include "ajbox_library.h"

enum { AJAPP_DRUMS, AJAPP_MELODY, AJAPP_SIDES };
enum { AJAPP_HOLD_FILL, AJAPP_HOLD_FAST, AJAPP_HOLD_MUTE_DRUMS, AJAPP_HOLD_MUTE_MELODY, AJAPP_HOLD_STUTTER, AJAPP_HOLDS };
enum { AJAPP_FX_ECHO, AJAPP_FX_SPARKLE, AJAPP_FX_WOBBLE, AJAPP_FX_WAH, AJAPP_FX_CRUNCH, AJAPP_FX_STUTTER, AJAPP_EFFECTS };
enum { AJAPP_KNOB_SWOOSH, AJAPP_KNOB_AMOUNT, AJAPP_KNOB_SPEED, AJAPP_KNOB_BALANCE, AJAPP_KNOBS };
enum { AJAPP_MOODS = 7, AJAPP_BPM_MIN = 30, AJAPP_BPM_MAX = 200, AJAPP_BPM_DEFAULT = 110 };

/* Call once after the stock instrument has started and its sample bank is bound. */
void ajapp_init(const ajb_library_t *library);
/* Call often from the control side (every few milliseconds): it finishes
 * things that wait for a beat or a bar. */
void ajapp_tick(void);
/* The finished stereo block (signed Q23, interleaved): AJ Box's own mix effects. */
void ajapp_audio(int32_t *block, unsigned frames);

void ajapp_press(unsigned side, unsigned slot);      /* a loop key on the side's page */
void ajapp_pick(unsigned side, unsigned index);      /* page * 8 + slot */
void ajapp_page_step(unsigned side, int delta);
void ajapp_dice(unsigned side);
void ajapp_hold(unsigned hold, int down);
void ajapp_finish(void);
void ajapp_play_stop(void);
void ajapp_home(void);
void ajapp_effect(unsigned effect, int on);
void ajapp_knob(unsigned knob, unsigned value);      /* 0..127 */
void ajapp_set_bpm(unsigned bpm);
void ajapp_set_kit(unsigned kit);
void ajapp_set_instrument(unsigned instrument);
void ajapp_set_octave(int octave);                   /* -2..+2 */
void ajapp_set_mood(unsigned mood);

typedef struct {
    int8_t slot[AJAPP_SIDES];           /* playing loop as page * 8 + slot, -1 none */
    uint8_t view_page[AJAPP_SIDES];
    uint8_t stopping[AJAPP_SIDES], muted[AJAPP_SIDES];
    uint8_t running, finishing, fill, fast;
    uint8_t step;                       /* 0..63 in the 4-bar cycle */
    uint8_t effects, mood, kit, instrument;
    int8_t octave;
    uint16_t bpm;
} ajapp_status_t;
void ajapp_status(ajapp_status_t *status);

/* Tuning aid: a track volume for both sides in place of the table (< 0 = table). */
void ajapp_debug_volume(int volume);
#endif
