/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * The 6 whole-mix effects and the Swoosh filter (PRD "Effects"). Echo and
 * Sparkle drive FM1 Open's own delay and reverb (web/engine/effects.c);
 * Wobble, Wah, Crunch, Stutter and Swoosh are new, each with its kid-safe
 * limit built in. Every switch and knob ramps, so nothing clicks. */
#ifndef AJB_EFFECTS_H
#define AJB_EFFECTS_H
#include <stdint.h>
#include "ajbox.h"
#include "dsp_math.h"

/* Stutter keeps the last 2 repeats of history. A 1/8 note is 12,027 samples
 * at 110 BPM; slower tempos repeat 1/16 or 1/32 so it always fits (24 KiB
 * of RAM per repeat length; measure on device). */
enum { AJB_STUTTER_MAX = 12288, AJB_STUTTER_HISTORY = 2 * AJB_STUTTER_MAX };

typedef struct {
    uint8_t on, last;                       /* bit per effect; last switched on */
    uint8_t amount[AJB_EFFECTS], speed[AJB_EFFECTS], swoosh;
    float wet[AJB_EFFECTS];                 /* ramps toward on/off */
    float beats;                            /* beat position, for synced LFOs */

    dsp_svf_t swoosh_lp[2], swoosh_hp[2];
    float swoosh_lp_hz, swoosh_hp_hz;
    float phase_x[6], phase_y[6], phase_feedback;
    dsp_svf_t wah;
    float crunch_hold, crunch_dry, crunch_wet;   /* held sample; clean and gritty power */
    unsigned crunch_count;

    int16_t history[AJB_STUTTER_HISTORY];
    uint32_t write_at, filled;              /* next history slot; samples kept */
    uint32_t loop_start, loop_length, loop_at;
    uint8_t stuttering;

    int32_t rev[AJB_BLOCK], dly[AJB_BLOCK];
} ajb_fx_t;

void ajb_fx_init(ajb_fx_t *fx);
void ajb_fx_switch(ajb_fx_t *fx, unsigned effect, int on, unsigned bpm, float beats);
/* KNOB 2 (AJB_KNOB_AMOUNT) or 3 (AJB_KNOB_SPEED) for the last effect on. */
void ajb_fx_knob(ajb_fx_t *fx, unsigned knob, unsigned value);
void ajb_fx_swoosh(ajb_fx_t *fx, unsigned value);
int ajb_fx_swoosh_clean(unsigned value);
/* One block: `mono` Q24 in; `left` and `right` Q24 out. `beats` is the beat
 * position at the block start, for synced LFOs and the Stutter grid. */
void ajb_fx_process(ajb_fx_t *fx, const int32_t *mono, int32_t *left, int32_t *right, unsigned bpm,
                    float beats);
#endif
