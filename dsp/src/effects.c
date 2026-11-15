/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh. */
#include "ajbox_target.h"
#include "effects.h"
#ifndef AJB_FX_STOCK_SENDS
#include "mixer.h"
#endif
#include "fixed.h"
#include <string.h>

void fm1_fx_set(int32_t decay, int32_t feedback, int32_t level, unsigned delay);   /* web/engine/effects.c */

/* On the FM-1 the sound interrupt may run while flash is being written, so
 * ajb_fx_process, everything it calls and its tables live in fast memory. */
#define AUDIO FM1_DSP_CODE
#define AUDIO_TABLE FM1_DSP_DATA

enum { RAMP = AJB_FADE_SAMPLES, EDGE = 32, CONTROL = 16 };
static const float FULL = 16777216.0f;          /* Q24 one */
static const float CRUNCH_FOLLOW = 1.0f / (0.02f * AJB_RATE);

static AUDIO float knob(uint8_t value) { return value / 127.0f; }
static AUDIO unsigned zone(uint8_t value, unsigned zones) { return value * zones / 128U; }

void ajb_fx_init(ajb_fx_t *fx)
{
    memset(fx, 0, sizeof *fx);
    for (unsigned k = 0; k < AJB_EFFECTS; ++k) fx->amount[k] = fx->speed[k] = 64;
    fx->speed[AJB_FX_STUTTER] = 20;             /* a 1/8 note, as the PRD says */
    fx->amount[AJB_FX_STUTTER] = 127;           /* the repeat replaces the loop */
    fx->swoosh = 64;
    fx->swoosh_lp_hz = 18000.0f;
    fx->swoosh_hp_hz = 20.0f;
    for (unsigned c = 0; c < 2; ++c) {
        dsp_svf_set(&fx->swoosh_lp[c], fx->swoosh_lp_hz, 0.8f);
        dsp_svf_set(&fx->swoosh_hp[c], fx->swoosh_hp_hz, 0.8f);
    }
    dsp_svf_set(&fx->wah, 800.0f, 2.5f);
#ifndef AJB_FX_STOCK_SENDS      /* on the stock instrument Echo and Sparkle are its own delay and reverb */
    fm1_fx_clear();
#endif
}

/* Samples in one Stutter repeat: the speed knob picks 1/8, 1/16 or 1/32,
 * halved further at slow tempos so it fits the history. */
static AUDIO uint32_t stutter_length(const ajb_fx_t *fx, unsigned bpm, unsigned *per_beat)
{
    unsigned division = 2U << zone(fx->speed[AJB_FX_STUTTER], 3);   /* repeats per beat */
    uint32_t length = (uint32_t)(AJB_RATE * 60U / bpm / division);
    while (length > AJB_STUTTER_MAX) { length /= 2U; division *= 2U; }
    *per_beat = division;
    return length;
}

void ajb_fx_switch(ajb_fx_t *fx, unsigned effect, int on, unsigned bpm, float beats)
{
    if (effect >= AJB_EFFECTS) return;
    if (on) {
        fx->on |= (uint8_t)(1U << effect);
        fx->last = (uint8_t)effect;
    } else {
        fx->on &= (uint8_t)~(1U << effect);
    }
    if (effect == AJB_FX_STUTTER && on && !fx->stuttering) {
        /* Repeat the grid slot that just finished, keeping the grid phase:
         * the repeat continues exactly where live audio would be. */
        unsigned per_beat;
        uint32_t length = stutter_length(fx, bpm, &per_beat);
        float grid = beats * (float)per_beat;
        uint32_t since = (uint32_t)((grid - (float)(int64_t)grid) * (float)length);
        if (since >= length) since = length - 1U;
        if (fx->filled < length + since) return;    /* not enough history yet */
        fx->loop_length = length;
        fx->loop_start = (fx->write_at + AJB_STUTTER_HISTORY - since - length) % AJB_STUTTER_HISTORY;
        fx->loop_at = since;
        fx->stuttering = 1;
    }
}

void ajb_fx_knob(ajb_fx_t *fx, unsigned which, unsigned value)
{
    if (value > 127) value = 127;
    if (which == AJB_KNOB_AMOUNT) fx->amount[fx->last] = (uint8_t)value;
    else if (which == AJB_KNOB_SPEED) fx->speed[fx->last] = (uint8_t)value;
}

/* KNOB 1: the encoders have no centre stop, so 63..65 snap to clean. */
int ajb_fx_swoosh_clean(unsigned value) { return value >= 63 && value <= 65; }
void ajb_fx_swoosh(ajb_fx_t *fx, unsigned value) { fx->swoosh = (uint8_t)(value > 127 ? 127 : value); }

static AUDIO float lfo(float beats, float period)
{
    float p = beats / period;
    p -= (float)(int32_t)p;     /* 32 bits: the 64-bit conversion is a flash routine, off limits to the sound interrupt */
    return 0.5f - 0.5f * dsp_cos(2.0f * DSP_PI * p);   /* 0..1, starts at 0 on the bar */
}

static AUDIO void swoosh_targets(unsigned v, float *lp, float *hp)
{
    *lp = 18000.0f;
    *hp = 20.0f;
    if (v < 63) *lp = 18000.0f * dsp_exp2(dsp_log2(300.0f / 18000.0f) * (63 - v) / 63.0f);
    if (v > 65) {
        float t = (v - 65) / 62.0f;
        *lp = 18000.0f * dsp_exp2(dsp_log2(4000.0f / 18000.0f) * t);   /* a tiny radio: */
        *hp = 20.0f * dsp_exp2(dsp_log2(1500.0f / 20.0f) * t);          /* no lows, no highs */
    }
}

AUDIO void ajb_fx_process(ajb_fx_t *fx, const int32_t *mono, int32_t *left, int32_t *right, unsigned bpm,
                    float beats)
{
    const float beat_step = (float)bpm / (60.0f * AJB_RATE);
    float start[AJB_EFFECTS], end[AJB_EFFECTS];
    for (unsigned k = 0; k < AJB_EFFECTS; ++k) {
        float target = (fx->on >> k) & 1U ? 1.0f : 0.0f;
        start[k] = fx->wet[k];
        float step = (float)AJB_BLOCK / RAMP;
        fx->wet[k] = target > start[k] ? dsp_min(target, start[k] + step) : dsp_max(target, start[k] - step);
        end[k] = fx->wet[k];
    }

    /* Echo: tempo-synced FM1 delay, feedback at most 60%. Sparkle: FM1
     * reverb, mix at most 50%. */
    float echo = knob(fx->amount[AJB_FX_ECHO]), sparkle = knob(fx->amount[AJB_FX_SPARKLE]);
#ifndef AJB_FX_STOCK_SENDS
    static const float echo_beats[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    unsigned delay = (unsigned)(AJB_RATE * 60.0f / bpm * echo_beats[zone(fx->speed[AJB_FX_ECHO], 4)] / 8.0f + 0.5f);
    if (delay >= FM1_FX_DELAY_SAMPLES) delay = FM1_FX_DELAY_SAMPLES - 1U;
    float feedback = 0.15f + 0.45f * echo;
    float decay = 0.88f - 0.33f * knob(fx->speed[AJB_FX_SPARKLE]);
    fm1_fx_set((int32_t)(decay * FM_Q30_ONE), (int32_t)(feedback * FM_Q30_ONE), FM_Q30_ONE, delay);
#endif
    float echo_send = 0.25f + 0.35f * echo, sparkle_send = 0.5f * sparkle;

    unsigned per_beat;
    (void)stutter_length(fx, bpm, &per_beat);
    float stutter_level = 0.5f + 0.5f * knob(fx->amount[AJB_FX_STUTTER]);

    float crunch = knob(fx->amount[AJB_FX_CRUNCH]);
    float drive = 1.0f + 11.0f * crunch;
    float levels = dsp_exp2(11.0f - 6.0f * crunch);   /* 12 down to 6 bits */
    unsigned hold = 1U + zone(fx->speed[AJB_FX_CRUNCH], 6);

    static const float wobble_beats[4] AUDIO_TABLE = {8.0f, 4.0f, 2.0f, 1.0f};
    static const float wah_beats[4] AUDIO_TABLE = {4.0f, 2.0f, 1.0f, 0.5f};
    float wobble_depth = 1.0f + 3.0f * knob(fx->amount[AJB_FX_WOBBLE]);      /* octaves, capped */
    float wobble_period = wobble_beats[zone(fx->speed[AJB_FX_WOBBLE], 4)];
    float wah_depth = 1.0f + 2.5f * knob(fx->amount[AJB_FX_WAH]);
    float wah_q = 2.5f + 1.5f * knob(fx->amount[AJB_FX_WAH]);                 /* never whistles */
    float wah_period = wah_beats[zone(fx->speed[AJB_FX_WAH], 4)];
    float phaser_a = 0.0f;

    for (unsigned i = 0; i < AJB_BLOCK; ++i) {
        float t = (float)i / AJB_BLOCK, b = beats + beat_step * (float)i;
        float w[AJB_EFFECTS];
        for (unsigned k = 0; k < AJB_EFFECTS; ++k) w[k] = start[k] + (end[k] - start[k]) * t;
        float x = (float)mono[i] / FULL;

        /* Stutter: freeze the history and loop the grid slot. */
        if (fx->stuttering) {
            uint32_t at = fx->loop_at, length = fx->loop_length;
            float rep = fx->history[(fx->loop_start + at) % AJB_STUTTER_HISTORY] * (1024.0f / FULL);
            float edge = dsp_min(1.0f, dsp_min((float)at, (float)(length - 1U - at)) / EDGE);
            float mix = w[AJB_FX_STUTTER] * stutter_level;
            x = x * (1.0f - mix) + rep * edge * mix;
            fx->loop_at = at + 1U == length ? 0U : at + 1U;
            if (!(fx->on & (1U << AJB_FX_STUTTER)) && end[AJB_FX_STUTTER] == 0.0f && i == AJB_BLOCK - 1U)
                fx->stuttering = 0;
        } else {
            float v = (float)mono[i] / 1024.0f;
            fx->history[fx->write_at] = (int16_t)dsp_clamp(v, -32768.0f, 32767.0f);
            if (++fx->write_at == AJB_STUTTER_HISTORY) fx->write_at = 0;
            if (fx->filled < AJB_STUTTER_HISTORY) ++fx->filled;
        }

        /* Crunch: soft clip, fewer bits, held samples. Two 20 ms power
         * followers turn the grit down to the clean level: never louder. */
        if (w[AJB_FX_CRUNCH] > 0.0f) {
            if (fx->crunch_count == 0) {
                float y = dsp_tanh(drive * x);
                fx->crunch_hold = (float)(int32_t)(y * levels + (y < 0 ? -0.5f : 0.5f)) / levels;
            }
            if (++fx->crunch_count >= hold) fx->crunch_count = 0;
            float y = fx->crunch_hold;
            fx->crunch_dry += CRUNCH_FOLLOW * (x * x - fx->crunch_dry);
            fx->crunch_wet += CRUNCH_FOLLOW * (y * y - fx->crunch_wet);
            if (fx->crunch_wet > fx->crunch_dry) y *= dsp_sqrt(fx->crunch_dry / fx->crunch_wet);
            x += w[AJB_FX_CRUNCH] * (y - x);
        } else {
            fx->crunch_dry = fx->crunch_wet = 0.0f;
        }

        /* Wobble: 4-stage phaser swept by a synced LFO, gentle feedback. */
        if (w[AJB_FX_WOBBLE] > 0.0f) {
            if (i % CONTROL == 0) {
                float hz = 200.0f * dsp_exp2(wobble_depth * lfo(b, wobble_period));
                float g = dsp_tan(DSP_PI * dsp_min(hz, 12000.0f) / AJB_RATE);
                phaser_a = (g - 1.0f) / (g + 1.0f);
            }
            float y = x + 0.5f * fx->phase_feedback;
            for (unsigned s = 0; s < 4; ++s) {
                float out = phaser_a * y + fx->phase_x[s] - phaser_a * fx->phase_y[s];
                fx->phase_x[s] = y;
                fx->phase_y[s] = out;
                y = out;
            }
            fx->phase_feedback = y;
            x += w[AJB_FX_WOBBLE] * (0.5f * (x + y) - x);
        } else if (fx->phase_feedback != 0.0f) {
            memset(fx->phase_x, 0, sizeof fx->phase_x);
            memset(fx->phase_y, 0, sizeof fx->phase_y);
            fx->phase_feedback = 0.0f;
        }

        /* Wah: resonant band-pass swept with the beat, peak gain one. */
        if (w[AJB_FX_WAH] > 0.0f) {
            if (i % CONTROL == 0) dsp_svf_set(&fx->wah, 350.0f * dsp_exp2(wah_depth * lfo(b, wah_period)), wah_q);
            float y = dsp_svf(&fx->wah, x, 1) * fx->wah.r * 1.2f;
            x += w[AJB_FX_WAH] * (y - x);
        }

        int32_t q = (int32_t)dsp_clamp(x * FULL, -2147483520.0f, 2147483520.0f);
        left[i] = right[i] = q;
        fx->dly[i] = (int32_t)(q * echo_send * w[AJB_FX_ECHO]);
        fx->rev[i] = (int32_t)(q * sparkle_send * w[AJB_FX_SPARKLE]);
    }
#ifndef AJB_FX_STOCK_SENDS
    fm1_fx_render(left, right, fx->rev, fx->dly);
#endif

    /* Swoosh (KNOB 1), on everything including echoes. */
    float lp_target, hp_target;
    swoosh_targets(fx->swoosh, &lp_target, &hp_target);
    fx->swoosh_lp_hz = dsp_exp2(dsp_log2(fx->swoosh_lp_hz) + 0.25f * (dsp_log2(lp_target) - dsp_log2(fx->swoosh_lp_hz)));
    fx->swoosh_hp_hz = dsp_exp2(dsp_log2(fx->swoosh_hp_hz) + 0.25f * (dsp_log2(hp_target) - dsp_log2(fx->swoosh_hp_hz)));
    int clean = ajb_fx_swoosh_clean(fx->swoosh) && fx->swoosh_lp_hz > 17900.0f && fx->swoosh_hp_hz < 20.5f;
    if (clean) {
        fx->swoosh_lp_hz = 18000.0f;
        fx->swoosh_hp_hz = 20.0f;
        for (unsigned c = 0; c < 2; ++c) {
            memset(&fx->swoosh_lp[c], 0, sizeof fx->swoosh_lp[c]);
            memset(&fx->swoosh_hp[c], 0, sizeof fx->swoosh_hp[c]);
            dsp_svf_set(&fx->swoosh_lp[c], 18000.0f, 0.8f);
            dsp_svf_set(&fx->swoosh_hp[c], 20.0f, 0.8f);
        }
        return;
    }
    int32_t *channel[2] = {left, right};
    for (unsigned c = 0; c < 2; ++c) {
        dsp_svf_set(&fx->swoosh_lp[c], fx->swoosh_lp_hz, 0.8f);
        dsp_svf_set(&fx->swoosh_hp[c], fx->swoosh_hp_hz, 0.8f);
        for (unsigned i = 0; i < AJB_BLOCK; ++i) {
            float x = (float)channel[c][i] / FULL;
            x = dsp_svf(&fx->swoosh_hp[c], dsp_svf(&fx->swoosh_lp[c], x, 0), 2);
            channel[c][i] = (int32_t)dsp_clamp(x * FULL, -2147483520.0f, 2147483520.0f);
        }
    }
}
