/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Master clock in 16th-note steps (PRD "How it fits together", Clock).
 *
 * Phase advances by `bpm` per sample; a 16th step is 44100 * 60 / 4 =
 * 661500 phase units, so step k starts at the first sample whose phase
 * reaches k * 661500. Integer arithmetic keeps every step exact with no
 * drift, and a tempo change keeps the position inside the current step. */
#ifndef AJB_CLOCK_H
#define AJB_CLOCK_H
#include <stdint.h>

#define AJB_STEP_PHASE UINT64_C(661500)

typedef struct {
    uint64_t phase, elapsed, next_step, last_start;
    unsigned bpm, running;
} ajb_clock_t;

static inline void ajb_clock_init(ajb_clock_t *c, unsigned bpm)
{
    c->phase = c->elapsed = c->next_step = c->last_start = 0;
    c->bpm = bpm;
    c->running = 0;
}
/* Start from silence: step 0 begins at the current sample. */
static inline void ajb_clock_start(ajb_clock_t *c)
{
    c->phase = c->elapsed = c->next_step = c->last_start = 0;
    c->running = 1;
}
/* Resume mid-cycle (jam replay): step `step` began `offset` samples ago. */
static inline void ajb_clock_start_at(ajb_clock_t *c, uint64_t step, unsigned offset)
{
    c->phase = step * AJB_STEP_PHASE + (uint64_t)offset * c->bpm;
    if (c->phase >= (step + 1U) * AJB_STEP_PHASE) c->phase = (step + 1U) * AJB_STEP_PHASE - 1U;
    c->elapsed = offset;
    c->last_start = 0;
    c->next_step = step + 1U;
    c->running = 1;
}
/* Samples from now until next_step begins (0 if it begins now). */
static inline uint64_t ajb_clock_until_next(const ajb_clock_t *c)
{
    uint64_t target = c->next_step * AJB_STEP_PHASE;
    return c->phase >= target ? 0 : (target - c->phase + c->bpm - 1) / c->bpm;
}
/* Consume next_step; returns its index and records its exact start. */
static inline uint64_t ajb_clock_fire(ajb_clock_t *c)
{
    c->last_start = c->elapsed + ajb_clock_until_next(c);
    return c->next_step++;
}
static inline void ajb_clock_advance(ajb_clock_t *c, unsigned samples)
{
    c->phase += (uint64_t)c->bpm * samples;
    c->elapsed += samples;
}
#endif
