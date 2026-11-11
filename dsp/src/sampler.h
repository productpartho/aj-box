/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * One SEA5 sample voice: a slice of a loop at its own pitch, or a whole note
 * at any pitch (PRD "How it fits together": 1 sliced drum voice and 1 pitched
 * melody voice). Decodes 20 frames at a time as it plays. */
#ifndef AJB_SAMPLER_H
#define AJB_SAMPLER_H
#include "ajbox_library.h"
#include "sea5.h"

typedef struct {
    const ajb_sample_t *sample;
    int32_t state[8];             /* SEA5 predictor after `group` */
    int16_t pcm[FM1_SEA_GROUP];   /* decoded frames of `group` */
    uint32_t group;               /* global group index in pcm, UINT32_MAX = none */
    uint32_t next, end;           /* next frame to read, end of the region */
    uint32_t frac, step;          /* Q16 read position between s0 and s1, rate */
    int32_t s0, s1, gain;         /* 16-bit samples, Q15 gain */
    int32_t last, declick;        /* Q24: last output, offset fading to zero */
    uint8_t active, done;
} ajb_voice_t;

void ajb_voice_reset(ajb_voice_t *v);
/* Slice `slice` of a sliced loop at its recorded pitch. */
void ajb_voice_slice(ajb_voice_t *v, const ajb_sample_t *s, unsigned slice, int32_t gain_q15);
/* The whole sample, pitched so root_note plays at MIDI `note`. */
void ajb_voice_note(ajb_voice_t *v, const ajb_sample_t *s, int note, int32_t gain_q15);
/* Stop at once; the declick ramp removes the step. */
void ajb_voice_stop(ajb_voice_t *v);
/* Add `frames` Q24 samples at AJB_RATE to bus. */
void ajb_voice_render(ajb_voice_t *v, int32_t *bus, unsigned frames);
/* One source frame (test and tools). Decodes from the frame's chunk start
 * unless it follows the previous read. */
int32_t ajb_voice_source(ajb_voice_t *v, uint32_t frame);
#endif
