/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh. */
#include "sampler.h"
#include "ajbox.h"
#include "dsp_math.h"
#include <string.h>

enum { END_FADE = 32 };       /* source frames faded at a region's end */

void ajb_voice_reset(ajb_voice_t *v)
{
    memset(v, 0, sizeof *v);
    v->group = UINT32_MAX;
}

int32_t ajb_voice_source(ajb_voice_t *v, uint32_t frame)
{
    uint32_t group = frame / FM1_SEA_GROUP;
    if (group != v->group) {
        /* Continue from the previous group of the same chunk, or reseed. */
        unsigned g = group % AJB_SEA_GROUPS;
        unsigned first = g && group == v->group + 1U ? g : 0U;
        const uint8_t *chunk = v->sample->data + (size_t)(frame / FM1_SEA_FRAMES) * AJB_SEA_CHUNK_BYTES;
        if (fm1_sea_decode_groups(chunk, AJB_SEA_CHUNK_BYTES, AJB_SEA_BITS, ajb_sea5_dqt(), first, g,
                                  v->state, v->pcm))
            memset(v->pcm, 0, sizeof v->pcm);
        v->group = group;
    }
    return v->pcm[frame % FM1_SEA_GROUP];
}

static void start(ajb_voice_t *v, const ajb_sample_t *s, uint32_t begin, uint32_t end, uint32_t step,
                  int32_t gain)
{
    /* Whatever was sounding becomes an offset that decays to zero, so a
     * retrigger never jumps. */
    v->declick = v->active || v->declick ? v->last : 0;
    v->sample = s;
    v->group = UINT32_MAX;
    v->next = begin;
    v->end = end;
    v->frac = 0;
    v->step = step;
    v->gain = gain;
    v->s0 = 0;
    v->s1 = begin < end ? ajb_voice_source(v, v->next++) : 0;
    v->active = begin < end;
    v->done = 0;
}

void ajb_voice_slice(ajb_voice_t *v, const ajb_sample_t *s, unsigned slice, int32_t gain_q15)
{
    if (!s || !s->slices) return;
    slice %= s->slices;
    uint32_t begin = ajb_slice_start(s->frames, s->slices, slice);
    uint32_t end = ajb_slice_start(s->frames, s->slices, slice + 1U);
    uint32_t step = (uint32_t)(((uint64_t)s->rate << 16) / AJB_RATE);
    start(v, s, begin, end, step, gain_q15);
}

void ajb_voice_note(ajb_voice_t *v, const ajb_sample_t *s, int note, int32_t gain_q15)
{
    if (!s) return;
    float ratio = dsp_exp2((float)(note - (int)s->root_note) / 12.0f);
    uint32_t step = (uint32_t)((float)s->rate / AJB_RATE * ratio * 65536.0f + 0.5f);
    start(v, s, 0, s->frames, step, gain_q15);
}

void ajb_voice_stop(ajb_voice_t *v)
{
    if (v->active) v->declick = v->last;
    v->active = 0;
}

void ajb_voice_render(ajb_voice_t *v, int32_t *bus, unsigned frames)
{
    for (unsigned i = 0; i < frames; ++i) {
        int32_t out = 0;
        if (v->active) {
            int32_t x = v->s0 + (int32_t)(((int64_t)(v->s1 - v->s0) * v->frac) >> 16);
            uint32_t left = v->end - v->next;
            int32_t fade = left < END_FADE ? (int32_t)left : END_FADE;
            /* 16-bit sample << 9 is Q24; Q15 gain; END_FADE = 32 = 2^5. */
            out = (int32_t)(((int64_t)x * v->gain * fade) >> 11);
            v->frac += v->step;
            while (v->frac >= 65536U && v->active) {
                v->frac -= 65536U;
                v->s0 = v->s1;
                if (v->next < v->end)
                    v->s1 = ajb_voice_source(v, v->next++);
                else { v->active = 0; v->done = 1; }
            }
        }
        out += v->declick;
        v->declick -= v->declick / 32;   /* about 0.7 ms */
        if (v->declick > -32 && v->declick < 32) v->declick = 0;
        v->last = out;
        bus[i] += out;
    }
}
