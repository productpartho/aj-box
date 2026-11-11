/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Writes synthesized stand-ins for the PRD's 8 recorded sounds (PRD "The 8
 * sampled sounds"), at 32 kHz mono, so the sampler can be heard and tested
 * before the real recordings exist:
 *   musicbox, kalimba, la, cat      one 0.8 s note at C5
 *   pots, dhol, beatbox, rainstick  one bar at 100 BPM, to be cut into 16 slices
 * Each is written as <name>_placeholder.wav.
 * Usage: make_placeholders <output directory> */
#include "wav.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RATE = 32000 };   /* the stock sample rate (SEA5) */
enum { NOTE = RATE * 8 / 10, STEP = RATE * 60 / 100 / 4, BAR = STEP * 16 };
static const double PI = 3.14159265358979323846;
static const double C5 = 523.2511;
static uint32_t noise_state = 0x1234567u;

static double noise(void)
{
    noise_state ^= noise_state << 13; noise_state ^= noise_state >> 17; noise_state ^= noise_state << 5;
    return (double)(noise_state >> 8) / 8388608.0 - 1.0;
}
static double uniform(double low, double high) { return low + (high - low) * (0.5 + 0.5 * noise()); }

static void normalize(double *x, unsigned n, double peak_out, int16_t *pcm)
{
    double peak = 1e-9;
    for (unsigned i = 0; i < n; ++i) peak = fabs(x[i]) > peak ? fabs(x[i]) : peak;
    for (unsigned i = 0; i < n; ++i) pcm[i] = (int16_t)lrint(x[i] / peak * peak_out * 32767.0);
}

/* A tine: fundamental, a weak octave and two short, bright cantilever modes. */
static void music_box(double *x, unsigned n, double f)
{
    static const double ratio[] = {1.0, 2.0, 5.40, 6.27};
    static const double amp[] = {1.0, 0.16, 0.22, 0.10};
    static const double tau[] = {0.45, 0.20, 0.06, 0.03};
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE, attack = t < 0.002 ? 0.5 - 0.5 * cos(PI * t / 0.002) : 1.0;
        double v = 0;
        for (unsigned p = 0; p < 4; ++p)
            v += amp[p] * exp(-t / tau[p]) * sin(2 * PI * f * ratio[p] * t);
        double tail = i > n - 160 ? (double)(n - i) / 160.0 : 1.0;
        x[i] += v * attack * tail;
    }
}

static void hit(double *x, unsigned n, unsigned at, double f, const double *ratios, unsigned modes,
                double tau, double level, double noise_level)
{
    for (unsigned i = at; i < n; ++i) {
        double t = (double)(i - at) / RATE, e = exp(-t / tau);
        if (e < 1e-4) break;
        double v = 0;
        for (unsigned m = 0; m < modes; ++m) v += sin(2 * PI * f * ratios[m] * t) / (m + 1.0);
        x[i] += level * e * (v + noise_level * noise() * exp(-t / (tau * 0.25)));
    }
}

/* A drum skin whose pitch falls from f0 to f1: dhol bass, a beatbox "b". */
static void thump(double *x, unsigned n, unsigned at, double f0, double f1, double glide, double tau,
                  double level)
{
    double phase = 0;
    for (unsigned i = at; i < n; ++i) {
        double t = (double)(i - at) / RATE, e = exp(-t / tau);
        if (e < 1e-4) break;
        phase += 2 * PI * (f1 + (f0 - f1) * exp(-t / glide)) / RATE;
        x[i] += level * e * (t < 0.001 ? t / 0.001 : 1.0) * sin(phase);
    }
}

/* Noise through a 2-pole resonator, about unit loudness at any tuning:
 * sticks, shakers, rain grains and beatbox consonants. */
static void hiss(double *x, unsigned n, unsigned at, double hz, double q, double tau, double level)
{
    double w = 2 * PI * hz / RATE, r = exp(-w / (2 * q)), a1 = 2 * r * cos(w), a2 = -r * r, y1 = 0, y2 = 0;
    double scale = sqrt(2 * (1 - r * r)) * sin(w) * 1.7;
    for (unsigned i = at; i < n; ++i) {
        double t = (double)(i - at) / RATE, e = exp(-t / tau);
        if (e < 1e-4) break;
        double y = noise() + a1 * y1 + a2 * y2;
        y2 = y1; y1 = y;
        x[i] += level * e * scale * y;
    }
}

/* A sung sound: harmonics of the note shaped by three formants that move
 * between keyframes (time, pitch ratio, formant Hz and levels). */
typedef struct { double t, pitch, hz[3], amp[3]; } sung_key_t;
static void sing(double *x, unsigned n, double f, const sung_key_t *key, unsigned keys, double tilt,
                 double vibrato, double attack, double fade_from)
{
    double phase = 0, end = (double)n / RATE;
    unsigned j = 0;
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE;
        while (j + 2 < keys && t >= key[j + 1].t) ++j;
        double u = (t - key[j].t) / (key[j + 1].t - key[j].t);
        u = u < 0 ? 0 : u > 1 ? 1 : u;
        double depth = vibrato * (t < 0.2 ? 0 : t < 0.4 ? (t - 0.2) / 0.2 : 1.0) * sin(2 * PI * 5.5 * t);
        double hz = f * (key[j].pitch + u * (key[j + 1].pitch - key[j].pitch)) * (1.0 + depth);
        phase += 2 * PI * hz / RATE;
        double v = 0;
        for (unsigned k = 1; k * hz < 14000.0; ++k) {
            double g = 0;
            for (unsigned m = 0; m < 3; ++m) {
                double centre = key[j].hz[m] + u * (key[j + 1].hz[m] - key[j].hz[m]);
                double d = (k * hz - centre) / (60.0 + 0.06 * centre);
                g += (key[j].amp[m] + u * (key[j + 1].amp[m] - key[j].amp[m])) / (1.0 + d * d);
            }
            v += g / pow((double)k, tilt) * sin(k * phase);
        }
        double env = t < attack ? 0.5 - 0.5 * cos(PI * t / attack) : 1.0;
        if (t > fade_from) env *= 0.5 + 0.5 * cos(PI * (t - fade_from) / (end - fade_from));
        x[i] += v * env;
    }
}

/* A kalimba: a round tine, one short inharmonic ping, a thumb tick and the
 * thock of the wooden box. */
static void kalimba(double *x, unsigned n, double f)
{
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE, attack = t < 0.004 ? 0.5 - 0.5 * cos(PI * t / 0.004) : 1.0;
        double v = exp(-t / 0.30) * sin(2 * PI * f * t) + 0.08 * exp(-t / 0.15) * sin(2 * PI * f * 2.0 * t) +
                   0.30 * exp(-t / 0.025) * sin(2 * PI * f * 6.27 * t) + 0.25 * exp(-t / 0.03) * sin(2 * PI * 190.0 * t);
        double tail = i > n - 160 ? (double)(n - i) / 160.0 : 1.0;
        x[i] += v * attack * tail;
    }
    hiss(x, n, 0, 2500.0, 1.0, 0.004, 0.25);
    hiss(x, n, 0, 4000.0, 3.0, 0.08, 0.04);    /* the buzz of the bottle caps */
}

static int save(const char *dir, const char *name, double *x, unsigned n)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s_placeholder.wav", dir, name);
    int16_t *pcm = malloc(n * sizeof *pcm);
    normalize(x, n, 0.7, pcm);
    ajb_wav_writer_t w;
    if (ajb_wav_open(&w, path, 1, RATE)) { fprintf(stderr, "cannot write %s\n", path); free(pcm); free(x); return 1; }
    ajb_wav_write(&w, pcm, n);
    ajb_wav_close(&w);
    printf("wrote %s (%u frames)\n", path, n);
    free(pcm);
    free(x);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: make_placeholders <dir>\n"); return 2; }
    int failed = 0;

    double *x = calloc(NOTE, sizeof *x);
    music_box(x, NOTE, C5);
    failed |= save(argv[1], "musicbox", x, NOTE);

    /* Pots and pans: two pots, a ringing pan and a lid ticking the off-16ths. */
    x = calloc(BAR, sizeof *x);
    static const double pot[] = {1.0, 2.32, 4.25}, pan[] = {1.0, 1.59, 2.14, 2.83};
    const char *pots = "x...x.x...x..x..", *pans = "..x...x...x...xX", *lid = "oooooooooooooooo";
    for (unsigned s = 0; s < 16; ++s) {
        if (pots[s] != '.') hit(x, BAR, s * STEP, 196.0, pot, 3, 0.09, 1.0, 0.3);
        if (pans[s] != '.') hit(x, BAR, s * STEP, 740.0, pan, 4, pans[s] == 'X' ? 0.25 : 0.12, 0.6, 0.6);
        if (lid[s] != '.') hit(x, BAR, s * STEP + STEP / 2, 2900.0, pan, 2, 0.015, 0.18, 2.0);
    }
    failed |= save(argv[1], "pots", x, BAR);

    /* Dhol, a bhangra chaal: the low skin booms, the high skin chatters. */
    x = calloc(BAR, sizeof *x);
    const char *low = "X.........X.X...", *high = "X.xoX.xoX.xoX.xo";
    for (unsigned s = 0; s < 16; ++s) {
        if (low[s] != '.') {
            thump(x, BAR, s * STEP, 140.0, 78.0, 0.06, 0.22, 1.0);
            hiss(x, BAR, s * STEP, 800.0, 1.0, 0.008, 0.3);
        }
        if (high[s] != '.') {
            double level = high[s] == 'X' ? 0.55 : high[s] == 'x' ? 0.4 : 0.22;
            hit(x, BAR, s * STEP, 330.0, pan, 4, 0.06, level, 0.8);
            hiss(x, BAR, s * STEP, 3500.0, 2.0, 0.012, level * 0.6);
        }
    }
    failed |= save(argv[1], "dhol", x, BAR);

    /* Beatbox: "b" lip kicks, "psh" snares and "ts" hats. */
    x = calloc(BAR, sizeof *x);
    const char *lips = "X......xX.......", *psh = "....X.......X...", *ts = "..x...x...x...xo";
    for (unsigned s = 0; s < 16; ++s) {
        if (lips[s] != '.') {
            thump(x, BAR, s * STEP, 170.0, 60.0, 0.03, 0.09, lips[s] == 'X' ? 1.0 : 0.7);
            hiss(x, BAR, s * STEP, 300.0, 1.5, 0.02, 0.5);
        }
        if (psh[s] != '.') {
            hiss(x, BAR, s * STEP, 1800.0, 1.2, 0.07, 0.8);
            hiss(x, BAR, s * STEP, 4500.0, 2.0, 0.04, 0.4);
            thump(x, BAR, s * STEP, 220.0, 180.0, 0.02, 0.03, 0.3);
        }
        if (ts[s] != '.') hiss(x, BAR, s * STEP, 7000.0, 3.0, 0.035, ts[s] == 'x' ? 0.45 : 0.25);
    }
    failed |= save(argv[1], "beatbox", x, BAR);

    /* Rain stick: hundreds of tiny seeds, swelling twice a bar, with a small
     * rush on every 8th so each slice has a start. */
    x = calloc(BAR, sizeof *x);
    for (unsigned i = 0; i < BAR; ++i) {
        double t = (double)i / RATE, density = 110.0 + 330.0 * (0.5 + 0.5 * cos(2 * PI * t / 1.2));
        if (0.5 + 0.5 * noise() < density / RATE)
            hiss(x, BAR, i, uniform(2500.0, 9000.0), 4.0, uniform(0.002, 0.008), uniform(0.2, 1.0));
    }
    for (unsigned s = 0; s < 16; s += 2)
        for (unsigned g = 0; g < 6; ++g)
            hiss(x, BAR, s * STEP + (unsigned)uniform(0, 0.015 * RATE), uniform(2000.0, 6000.0), 4.0,
                 uniform(0.004, 0.010), uniform(0.6, 1.2));
    failed |= save(argv[1], "rainstick", x, BAR);

    x = calloc(NOTE, sizeof *x);
    kalimba(x, NOTE, C5);
    failed |= save(argv[1], "kalimba", x, NOTE);

    /* "La": a quick L, then a child's bright "ah" with a little vibrato. */
    static const sung_key_t la[] = {
        {0.00, 0.97, {350, 1400, 2900}, {0.5, 0.20, 0.10}},
        {0.05, 1.00, {350, 1400, 2900}, {0.6, 0.25, 0.10}},
        {0.11, 1.00, {1000, 1500, 3300}, {1.0, 0.70, 0.35}},
        {0.80, 1.00, {1000, 1500, 3300}, {1.0, 0.70, 0.35}},
    };
    x = calloc(NOTE, sizeof *x);
    sing(x, NOTE, C5, la, 4, 1.0, 0.006, 0.025, 0.50);
    failed |= save(argv[1], "la", x, NOTE);

    /* Cat: "m-e-a-ow", the pitch sliding up into the note and off the end. */
    static const sung_key_t meow[] = {
        {0.00, 0.80, {300, 1100, 2500}, {0.6, 0.10, 0.05}},
        {0.06, 0.88, {300, 1100, 2500}, {0.8, 0.15, 0.05}},
        {0.18, 1.00, {450, 2400, 3200}, {1.0, 0.80, 0.40}},
        {0.40, 1.00, {950, 1600, 3000}, {1.0, 0.80, 0.30}},
        {0.75, 0.72, {400, 850, 2600}, {0.8, 0.35, 0.10}},
        {0.80, 0.70, {400, 850, 2600}, {0.8, 0.35, 0.10}},
    };
    x = calloc(NOTE, sizeof *x);
    sing(x, NOTE, C5, meow, 6, 0.9, 0.0, 0.05, 0.35);
    hiss(x, NOTE, 0, 3000.0, 1.0, 0.25, 0.02);
    failed |= save(argv[1], "cat", x, NOTE);
    return failed;
}
