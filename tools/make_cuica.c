/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Synthesizes a cuica (the squeaking friction drum in Goron City) as two
 * one-shot sounds for the stock sampler's pads, and a preview of the Goron
 * City cuica rhythm at 115 BPM. No recordings: PRD "The 8 sampled sounds".
 *
 *   make_cuica <out dir>   writes cuica-high.wav, cuica-low.wav (32 kHz mono)
 *                          and cuica-A-rhythm.wav, cuica-B-rhythm.wav, cuica-C-rhythm.wav
 *
 * A cuica is a rubbed stick pulling on a drum skin: a buzzy, voice-like
 * tone whose pitch slides as the player presses the skin. Here: a pulse
 * train with a sliding pitch through two vocal-style resonances, with a
 * little roughness from the friction and a short drum-body thump. */
#include "wav.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { RATE = 32000 };
static const double PI = 3.14159265358979323846;
static uint32_t seed = 0x2468ace1u;
static double noise(void)
{
    seed = seed * 1664525u + 1013904223u;
    return (double)(int32_t)seed / 2147483648.0;
}

/* Two-pole resonator, one sample. */
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

/* One squeak: pitch slides from f0 to f1 over `seconds`; `open` brightens it. */
static unsigned squeak(double *x, double f0, double f1, double seconds, double open)
{
    unsigned n = (unsigned)(seconds * RATE);
    resonator_t low = {0}, high = {0}, body = {0};
    double phase = 0.0;
    tune(&body, 180.0, 60.0);
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / n;
        double f = f0 + (f1 - f0) * (1.0 - exp(-4.0 * t)) / (1.0 - exp(-4.0));
        f *= 1.0 + 0.012 * sin(2.0 * PI * 31.0 * i / RATE) + 0.006 * noise();   /* stick-slip wobble */
        phase += f / RATE;
        if (phase >= 1.0) phase -= 1.0;
        double pulse = (phase < 0.22 ? 1.0 : -0.28) + 0.05 * noise();           /* narrow pulse: buzzy */
        tune(&low, f * 2.0 + 250.0 * open, 180.0);                               /* resonances follow the pitch */
        tune(&high, 1500.0 + 900.0 * open + f, 420.0);
        double attack = i < RATE / 200 ? (double)i / (RATE / 200) : 1.0;
        double env = attack * pow(1.0 - t, 1.6);
        double thump = i < RATE / 25 ? ring(&body, i == 0 ? 6.0 : 0.0) : ring(&body, 0.0);
        x[i] = env * (3.0 * ring(&low, pulse) + 2.2 * ring(&high, pulse) + 0.25 * pulse) + 0.6 * thump;
    }
    return n;
}

/* A purer, more voice-like squeak (versions B and C): a few sine partials with
 * a fast pitch slide, a breathy edge and a quick flutter. `rough` adds grit. */
static unsigned hoot(double *x, double f0, double f1, double seconds, double rough)
{
    unsigned n = (unsigned)(seconds * RATE);
    double phase = 0.0;
    resonator_t mouth = {0};
    for (unsigned i = 0; i < n; ++i) {
        double t = (double)i / n;
        double slide = t < 0.35 ? t / 0.35 : 1.0;                       /* most of the slide happens early */
        double f = f0 + (f1 - f0) * slide * slide * (3.0 - 2.0 * slide);
        f *= 1.0 + 0.02 * sin(2.0 * PI * 42.0 * i / RATE) + rough * 0.01 * noise();
        phase += 2.0 * PI * f / RATE;
        double tone = sin(phase) + 0.45 * sin(2.0 * phase) + 0.22 * sin(3.0 * phase) + rough * 0.3 * sin(5.0 * phase);
        tune(&mouth, 2.6 * f, 500.0);
        double attack = i < RATE / 80 ? (double)i / (RATE / 80) : 1.0;
        double env = attack * (t < 0.7 ? 1.0 : (1.0 - t) / 0.3);
        x[i] = env * (tone + 1.5 * ring(&mouth, tone + 0.2 * noise()));
    }
    return n;
}

static void normalise(const double *x, unsigned n, double peak, int16_t *pcm)
{
    double top = 1e-9;
    for (unsigned i = 0; i < n; ++i) top = fabs(x[i]) > top ? fabs(x[i]) : top;
    for (unsigned i = 0; i < n; ++i) pcm[i] = (int16_t)lrint(x[i] / top * peak * 32767.0);
}
static int save(const char *dir, const char *name, const int16_t *pcm, unsigned n)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    ajb_wav_writer_t w;
    if (ajb_wav_open(&w, path, 1, RATE)) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    ajb_wav_write(&w, pcm, n);
    ajb_wav_close(&w);
    printf("%s  %.2f s\n", path, (double)n / RATE);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: make_cuica <out dir>\n"); return 2; }
    static double high[RATE], low[RATE], mix[RATE * 10];
    static int16_t pcm[RATE * 10];
    unsigned nh = squeak(high, 430.0, 640.0, 0.20, 1.0);    /* the rising "hoo" */
    unsigned nl = squeak(low, 330.0, 270.0, 0.24, 0.0);     /* the falling "uh" */
    int failed = 0;
    normalise(high, nh, 0.9, pcm); failed |= save(argv[1], "cuica-high.wav", pcm, nh);
    normalise(low, nl, 0.9, pcm);  failed |= save(argv[1], "cuica-low.wav", pcm, nl);

    /* The cuica part of Goron City, bars 5 and 6 of the score, twice: 'h' high, 'l' low. */
    static const char rhythm[] = "l.h...h.l.l....." "..h.l.h.l.l.....";
    double step = RATE * 60.0 / 115.0 / 4.0;
    unsigned total = (unsigned)(step * 64) + RATE / 2;
    for (unsigned s = 0; s < 64; ++s) {
        char c = rhythm[s % 32];
        const double *src = c == 'h' ? high : c == 'l' ? low : NULL;
        unsigned n = c == 'h' ? nh : nl, at = (unsigned)(s * step);
        for (unsigned i = 0; src && i < n && at + i < total; ++i) mix[at + i] += src[i];
    }
    normalise(mix, total, 0.7, pcm);
    failed |= save(argv[1], "cuica-A-rhythm.wav", pcm, total);

    /* Versions B (clean, high) and C (a little gritty, wider slide) for comparison. */
    static const struct { const char *name; double h0, h1, l0, l1, rough; } more[2] = {
        {"cuica-B-rhythm.wav", 620.0, 980.0, 520.0, 400.0, 0.0},
        {"cuica-C-rhythm.wav", 500.0, 1150.0, 600.0, 330.0, 1.0}};
    for (unsigned v = 0; v < 2; ++v) {
        nh = hoot(high, more[v].h0, more[v].h1, 0.16, more[v].rough);
        nl = hoot(low, more[v].l0, more[v].l1, 0.20, more[v].rough);
        memset(mix, 0, sizeof mix);
        for (unsigned s = 0; s < 64; ++s) {
            char c = rhythm[s % 32];
            const double *src = c == 'h' ? high : c == 'l' ? low : NULL;
            unsigned n = c == 'h' ? nh : nl, at = (unsigned)(s * step);
            for (unsigned i = 0; src && i < n && at + i < total; ++i) mix[at + i] += src[i];
        }
        normalise(mix, total, 0.7, pcm);
        failed |= save(argv[1], more[v].name, pcm, total);
    }

    /* Partho picked A as closest. Three takes on A: higher, a wider and longer
     * slide, and both together. */
    static const struct { const char *name; double h0, h1, hs, l0, l1, ls; } takes[3] = {
        {"cuica-A2-higher.wav", 560.0, 860.0, 0.20, 440.0, 350.0, 0.24},
        {"cuica-A3-wider-longer.wav", 400.0, 820.0, 0.27, 380.0, 240.0, 0.30},
        {"cuica-A4-higher-wider.wav", 520.0, 1050.0, 0.25, 480.0, 300.0, 0.28}};
    for (unsigned v = 0; v < 3; ++v) {
        memset(high, 0, sizeof high); memset(low, 0, sizeof low);
        nh = squeak(high, takes[v].h0, takes[v].h1, takes[v].hs, 1.0);
        nl = squeak(low, takes[v].l0, takes[v].l1, takes[v].ls, 0.0);
        memset(mix, 0, sizeof mix);
        for (unsigned s = 0; s < 64; ++s) {
            char c = rhythm[s % 32];
            const double *src = c == 'h' ? high : c == 'l' ? low : NULL;
            unsigned n = c == 'h' ? nh : nl, at = (unsigned)(s * step);
            for (unsigned i = 0; src && i < n && at + i < total; ++i) mix[at + i] += src[i];
        }
        normalise(mix, total, 0.7, pcm);
        failed |= save(argv[1], takes[v].name, pcm, total);
    }
    return failed;
}
