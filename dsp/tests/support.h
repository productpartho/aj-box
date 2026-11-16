/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Shared test helpers: checks, event capture and rendering. */
#ifndef AJB_TEST_SUPPORT_H
#define AJB_TEST_SUPPORT_H
#include "ajbox.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>

extern unsigned checks, failures;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { ajb_event_t *items; size_t count, capacity; } events_t;
static inline void capture(void *context, const ajb_event_t *e)
{
    events_t *list = context;
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 256;
        list->items = realloc(list->items, list->capacity * sizeof *list->items);
    }
    list->items[list->count++] = *e;
}
static inline uint64_t step_start(uint64_t k, unsigned bpm) { return (k * AJB_STEP_PHASE + bpm - 1) / bpm; }

static inline int32_t render_blocks(ajb_engine_t *e, unsigned blocks)
{
    int32_t out[AJB_BLOCK * 2], peak = 0;
    for (unsigned b = 0; b < blocks; ++b) {
        ajb_render(e, out);
        for (unsigned i = 0; i < AJB_BLOCK * 2; ++i) peak = abs(out[i]) > peak ? abs(out[i]) : peak;
    }
    return peak;
}
static inline void render_until(ajb_engine_t *e, uint64_t sample)
{
    int32_t out[AJB_BLOCK * 2];
    while (ajb_now(e) < sample) ajb_render(e, out);
}
/* Render until `sample`, keeping the left channel in `mono` (n samples max). */
static inline size_t render_capture(ajb_engine_t *e, uint64_t sample, int32_t *mono, size_t n)
{
    int32_t out[AJB_BLOCK * 2];
    size_t count = 0;
    while (ajb_now(e) < sample) {
        ajb_render(e, out);
        for (unsigned i = 0; i < AJB_BLOCK && count < n; ++i) mono[count++] = out[2 * i];
    }
    return count;
}
static inline const ajb_event_t *first_of(const events_t *ev, unsigned side, unsigned slot, uint64_t after)
{
    for (size_t i = 0; i < ev->count; ++i)
        if (ev->items[i].kind == AJB_EVENT_STEP && ev->items[i].side == side && ev->items[i].slot == slot &&
            ev->items[i].sample >= after) return &ev->items[i];
    return NULL;
}
void test_s2(void);
void test_library(void);
#endif
