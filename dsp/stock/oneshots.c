/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * The 8 one-shot sounds for the stock sampler's pads (PRD "The 8 sampled
 * sounds"): all synthesized here, nothing recorded. 32 kHz mono, as the
 * stock sample bank wants. The cuica is the version Partho chose on
 * Nov 14, 2026 (tools/make_cuica.c, version A). */
#include "oneshots.h"
#include <math.h>
#include <string.h>

enum { RATE = 32000 };
static const double PI = 3.14159265358979323846;
static uint32_t seed;
static double noise(void)
{
    seed = seed * 1664525u + 1013904223u;
    return (double)(int32_t)seed / 2147483648.0;
}

/* A struck object: a few decaying partials and a burst of noise. */
static void hit(double *x, unsigned n, double f, const double *ratios, unsigned modes, double tau, double noise_level)
{
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE, e = exp(-t / tau), v = 0;
        for (unsigned m = 0; m < modes; ++m) v += sin(2 * PI * f * ratios[m] * t) / (m + 1.0);
        x[i] += e * (v + noise_level * noise() * exp(-t / (tau * 0.25)));
    }
}
/* A drum skin whose pitch falls from f0 to f1. */
static void thump(double *x, unsigned n, double f0, double f1, double glide, double tau, double level)
{
    double phase = 0;
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE;
        phase += 2 * PI * (f1 + (f0 - f1) * exp(-t / glide)) / RATE;
        x[i] += level * exp(-t / tau) * (t < 0.001 ? t / 0.001 : 1.0) * sin(phase);
    }
}
/* Noise through a resonance: sticks, shakers, mouth sounds. */
static void hiss(double *x, unsigned n, double hz, double q, double tau, double level)
{
    double w = 2 * PI * hz / RATE, r = exp(-w / (2 * q)), a1 = 2 * r * cos(w), a2 = -r * r, y1 = 0, y2 = 0;
    double scale = sqrt(2 * (1 - r * r)) * sin(w) * 1.7;
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / RATE, y = noise() + a1 * y1 + a2 * y2;
        y2 = y1; y1 = y;
        x[i] += level * exp(-t / tau) * scale * y;
    }
}

typedef struct { double a1, a2, gain, y1, y2; } resonator_t;
static void tune(resonator_t *r, double hz, double bandwidth)
{
    double radius = exp(-PI * bandwidth / RATE);
    r->a1 = 2.0 * radius * cos(2.0 * PI * hz / RATE);
    r->a2 = -radius * radius;
    r->gain = 1.0 - radius;
}
static double ring(resonator_t *r, double x)
{
    double y = r->gain * x + r->a1 * r->y1 + r->a2 * r->y2;
    r->y2 = r->y1; r->y1 = y;
    return y;
}
/* The cuica squeak: a buzzy tone with a sliding pitch through two voice-like resonances. */
static void squeak(double *x, unsigned n, double f0, double f1, double open)
{
    resonator_t low = {0}, high = {0}, body = {0};
    double phase = 0.0;
    tune(&body, 180.0, 60.0);
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / n;
        double f = f0 + (f1 - f0) * (1.0 - exp(-4.0 * t)) / (1.0 - exp(-4.0));
        f *= 1.0 + 0.012 * sin(2.0 * PI * 31.0 * i / RATE) + 0.006 * noise();
        phase += f / RATE;
        if (phase >= 1.0) phase -= 1.0;
        double pulse = (phase < 0.22 ? 1.0 : -0.28) + 0.05 * noise();
        tune(&low, f * 2.0 + 250.0 * open, 180.0);
        tune(&high, 1500.0 + 900.0 * open + f, 420.0);
        double attack = i < RATE / 200 ? (double)i / (RATE / 200) : 1.0;
        double thud = ring(&body, i == 0 ? 6.0 : 0.0);
        x[i] = attack * pow(1.0 - t, 1.6) * (3.0 * ring(&low, pulse) + 2.2 * ring(&high, pulse) + 0.25 * pulse) + 0.6 * thud;
    }
}

const char *const ajb_oneshot_names[AJB_ONESHOTS] = {"DholLo", "DholHi", "Pot", "Spoon", "BoxKick", "Shh", "CuicaHi", "CuicaLo"};

unsigned ajb_oneshot(unsigned pad, int16_t *pcm)
{
    static const double pot[5] = {1.0, 2.32, 3.87, 5.13, 6.9}, skin[3] = {1.0, 1.59, 2.14}, wood[3] = {1.0, 2.7, 5.1};
    static const double seconds[AJB_ONESHOTS] = {0.42, 0.16, 0.60, 0.08, 0.30, 0.22, 0.20, 0.24};
    static double x[AJB_ONESHOT_FRAMES];
    if (pad >= AJB_ONESHOTS) return 0;
    unsigned n = (unsigned)(seconds[pad] * RATE);
    memset(x, 0, sizeof x);
    seed = 0x2468ace1u + pad * 7919u;
    switch (pad) {
    case 0: thump(x, n, 190.0, 82.0, 0.035, 0.16, 1.0); hiss(x, n, 900.0, 1.2, 0.012, 0.25); break;       /* dhol bass side */
    case 1: hit(x, n, 520.0, skin, 3, 0.045, 0.9); hiss(x, n, 3200.0, 1.5, 0.02, 0.5); break;              /* dhol treble side */
    case 2: hit(x, n, 640.0, pot, 5, 0.19, 0.5); break;                                                    /* a pot lid */
    case 3: hit(x, n, 1900.0, wood, 3, 0.014, 1.2); break;                                                 /* spoons */
    case 4: thump(x, n, 160.0, 58.0, 0.03, 0.11, 1.0); hiss(x, n, 420.0, 0.9, 0.02, 0.5); break;           /* a mouth "b" */
    case 5: hiss(x, n, 5200.0, 0.8, 0.07, 1.0); hiss(x, n, 2400.0, 1.4, 0.04, 0.5); break;                 /* "psh", and rain grains */
    case 6: squeak(x, n, 430.0, 640.0, 1.0); break;                                                        /* cuica, rising */
    default: squeak(x, n, 330.0, 270.0, 0.0); break;                                                       /* cuica, falling */
    }
    double top = 1e-9;
    for (unsigned i = 0; i < n; ++i) top = fabs(x[i]) > top ? fabs(x[i]) : top;
    unsigned fade = RATE / 200;
    for (unsigned i = 0; i < n; ++i) {
        double g = i + fade >= n ? (double)(n - 1 - i) / fade : 1.0;        /* no click at the end */
        pcm[i] = (int16_t)lrint(x[i] / top * g * 0.9 * 32767.0);
    }
    return n;
}
