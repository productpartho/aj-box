/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Desktop tests for S1 (S2 controls are in test_s2.c): clock timing, swap steps, stopping,
 * output clipping, melody engines, moods, SEA5 and sampler pitch. Exit code 0 = all passed. */
#include "support.h"
#include "sampler.h"
#include "sea_encode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

unsigned checks, failures;


/* ---- 1. clock ---- */

static void test_clock(void)
{
    printf("clock: exact 16th steps, no drift, tempo changes keep phase\n");
    static const unsigned tempos[] = {30, 110, 111, 199, 200};
    for (unsigned t = 0; t < 5; ++t) {
        ajb_clock_t c;
        ajb_clock_init(&c, tempos[t]);
        ajb_clock_start(&c);
        uint32_t rng = 12345;
        unsigned bad = 0;
        while (c.next_step < 100000) {         /* up to 3.5 hours of music */
            rng = rng * 1664525U + 1013904223U;
            unsigned chunk = 1 + (rng >> 24) % 200;
            while (ajb_clock_until_next(&c) < chunk) {
                uint64_t k = ajb_clock_fire(&c);
                if (c.last_start != step_start(k, tempos[t])) ++bad;
            }
            ajb_clock_advance(&c, chunk);
        }
        CHECK(bad == 0, "%u BPM: %u of 100000 steps off their exact sample", tempos[t], bad);
    }
    /* Halfway through step 1000 at 110 BPM, change to 150: the rest of the
     * step takes the remaining half at the new tempo. */
    ajb_clock_t c;
    ajb_clock_init(&c, 110);
    ajb_clock_start(&c);
    uint64_t half = (1000 * AJB_STEP_PHASE + AJB_STEP_PHASE / 2) / 110;
    ajb_clock_advance(&c, (unsigned)half);
    c.next_step = 1001;
    c.bpm = 150;
    uint64_t left = 1001 * AJB_STEP_PHASE - c.phase;
    CHECK(ajb_clock_until_next(&c) == (left + 149) / 150, "tempo change lost the step phase");
    CHECK(llabs((long long)ajb_clock_until_next(&c) - (long long)(AJB_STEP_PHASE / 2 / 150)) <= 1,
          "half a step at 150 BPM should be about %llu samples", (unsigned long long)(AJB_STEP_PHASE / 2 / 150));
}

/* ---- 2. engine step timing ---- */

static void test_engine_timing(void)
{
    printf("engine: every step plays within one tick of its exact sample\n");
    static const unsigned tempos[] = {30, 110, 200};
    for (unsigned t = 0; t < 3; ++t) {
        events_t ev = {0};
        ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
        ajb_set_bpm(e, tempos[t]);
        ajb_set_event_hook(e, capture, &ev);
        ajb_press(e, AJB_DRUMS, 0);
        ajb_press(e, AJB_MELODY, 0);
        render_until(e, step_start(16 * 32, tempos[t]));   /* 32 bars */
        unsigned late = 0, early = 0, wrong = 0, gaps = 0;
        uint64_t expect[AJB_SIDES] = {0, 0};
        for (size_t i = 0; i < ev.count; ++i) {
            const ajb_event_t *x = &ev.items[i];
            if (x->sample > x->exact) ++late;
            if (x->exact - x->sample >= AJB_TICK) ++early;
            if (x->exact != step_start(x->global_step, tempos[t])) ++wrong;
            if (x->global_step != expect[x->side]) ++gaps;
            expect[x->side] = x->global_step + 1;
        }
        CHECK(ev.count >= 2 * 16 * 32, "%u BPM: only %zu steps played", tempos[t], ev.count);
        CHECK(ev.count && ev.items[0].global_step == 0 && ev.items[1].global_step == 0 &&
              ev.items[0].loop_step == 0 && ev.items[1].loop_step == 0,
              "%u BPM: both sides must start together on step 1 from silence", tempos[t]);
        CHECK(!late && !early && !wrong && !gaps, "%u BPM: late %u, early %u, wrong %u, gaps %u",
              tempos[t], late, early, wrong, gaps);
        ajb_engine_destroy(e);
        free(ev.items);
    }
}

/* ---- 3. onsets in the rendered audio ---- */

static const ajb_drum_loop_t kick_quarters = {"Kick", 16, NULL, {[AJB_BD] = "X...X...X...X..."}, 32768};
static const ajb_page_t kick_page = {"Test", {&kick_quarters}, {0}, 0};
static const ajb_library_t kick_library = {.pages = &kick_page, .page_count = 1};

static void test_onsets(void)
{
    printf("audio: kick onsets land on the beat grid with no drift\n");
    ajb_engine_t *e = ajb_engine_create(&kick_library);
    ajb_set_bpm(e, 110);
    ajb_press(e, AJB_DRUMS, 0);
    unsigned beats = 4 * 64;
    /* Stop before beat `beats`, which may play up to one tick early. */
    uint64_t total = step_start(4ULL * beats, 110) - AJB_TICK;
    int32_t out[AJB_BLOCK * 2];
    uint64_t onsets[512];
    /* Armed after 1500 samples below -34 dBFS; an onset is the first sample
     * above -20 dBFS once armed. */
    unsigned found = 0, quiet = 0, armed = 1;
    for (uint64_t n = 0; n < total;) {
        ajb_render(e, out);
        for (unsigned i = 0; i < AJB_BLOCK; ++i, ++n) {
            int32_t a = abs(out[2 * i]);
            if (a > 8388608 / 10 && armed && found < 512) { onsets[found++] = n; armed = 0; }
            quiet = a < 8388608 / 50 ? quiet + 1 : 0;
            if (quiet > 1500) armed = 1;
        }
    }
    CHECK(found == beats, "found %u kick onsets, expected %u", found, beats);
    long long worst = 0;
    for (unsigned b = 0; b < found; ++b) {
        long long d = (long long)(onsets[b] - onsets[0]) - (long long)(step_start(4ULL * b, 110) - step_start(0, 110));
        if (llabs(d) > worst) worst = llabs(d);
    }
    CHECK(worst < AJB_TICK + 4, "onset deviation %lld samples (limit %d)", worst, AJB_TICK + 4);
    printf("  %u onsets over %u bars, worst deviation %lld samples (%.2f ms)\n", found, beats / 4, worst,
           worst * 1000.0 / AJB_RATE);
    ajb_engine_destroy(e);
}

/* ---- 4. swap step ---- */


static void test_swap_step(void)
{
    printf("swaps: phase-locked to the clock step, no waiting for the bar\n");
    const unsigned bpm = 110;
    unsigned bad_drum = 0, bad_join = 0, leftovers = 0, grace_joins = 0;
    for (unsigned trial = 0; trial < 60; ++trial) {
        events_t ev = {0};
        ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
        ajb_set_bpm(e, bpm);
        ajb_set_event_hook(e, capture, &ev);
        ajb_press(e, AJB_DRUMS, 0);
        /* Spread the presses over every position inside a step. */
        render_until(e, step_start(37, bpm) + (uint64_t)trial * 6014 / 60);
        uint64_t press = ajb_now(e);
        ajb_press(e, AJB_DRUMS, 1);        /* Stomp, 32 steps */
        ajb_press(e, AJB_MELODY, 1);       /* Music Box Song, 64 steps */
        render_until(e, press + step_start(40, bpm));

        /* Drum swap: the next step after the press, at its loop position. */
        uint64_t next = 0;
        while (step_start(next, bpm) < press) ++next;
        const ajb_event_t *d = first_of(&ev, AJB_DRUMS, 1, press);
        if (!d || d->global_step != next || d->loop_step != next % 32 || d->exact != step_start(next, bpm)) ++bad_drum;
        for (size_t i = 0; i < ev.count; ++i)
            if (ev.items[i].side == AJB_DRUMS && ev.items[i].slot == 0 && ev.items[i].global_step >= next) ++leftovers;

        /* Melody joining a silent side: the current step inside the grace
         * window (played at the press), otherwise the next step. */
        uint64_t current = next - 1, age = press - step_start(current, bpm);
        int grace = age < AJB_GRACE_SAMPLES && step_start(next, bpm) - press >= AJB_TICK;
        uint64_t join = grace ? current : next;
        const ajb_event_t *m = first_of(&ev, AJB_MELODY, 1, press);
        if (!m || m->global_step != join || m->loop_step != join % 64 ||
            (grace ? m->sample != press : m->exact != step_start(next, bpm))) ++bad_join;
        grace_joins += grace;
        ajb_engine_destroy(e);
        free(ev.items);
    }
    CHECK(bad_drum == 0, "%u of 60 drum swaps started on the wrong step", bad_drum);
    CHECK(leftovers == 0, "old drum loop played %u steps after the swap", leftovers);
    CHECK(bad_join == 0, "%u of 60 melody joins started on the wrong step", bad_join);
    CHECK(grace_joins > 0 && grace_joins < 60, "grace window never or always used (%u)", grace_joins);

    /* 4-bar alignment: a 64-step loop joining at global step 37 plays step 37. */
    events_t ev = {0};
    ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
    ajb_set_event_hook(e, capture, &ev);
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(36, 110) + 3000);
    ajb_press(e, AJB_MELODY, 1);
    render_until(e, step_start(40, 110));
    const ajb_event_t *m = first_of(&ev, AJB_MELODY, 1, 0);
    CHECK(m && m->global_step == 37 && m->loop_step == 37, "4-bar loop joined at loop step %u, expected 37",
          m ? m->loop_step : 999);
    ajb_engine_destroy(e);
    free(ev.items);
}

/* ---- 5. same key stops on the next beat with a 10 ms fade ---- */

static void test_stop(void)
{
    printf("stop: same key again stops that side on the next beat, faded\n");
    events_t ev = {0};
    ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
    ajb_set_event_hook(e, capture, &ev);
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(21, 110) + 500);       /* inside beat 6 */
    ajb_press(e, AJB_DRUMS, 0);
    uint64_t beat = step_start(24, 110);
    int32_t out[AJB_BLOCK * 2];
    uint64_t last_sound = 0;
    while (ajb_now(e) < beat + AJB_RATE) {
        uint64_t at = ajb_now(e);
        ajb_render(e, out);
        for (unsigned i = 0; i < AJB_BLOCK; ++i)
            if (out[2 * i]) last_sound = at + i;
    }
    unsigned after = 0;
    for (size_t i = 0; i < ev.count; ++i) after += ev.items[i].global_step >= 24;
    CHECK(after == 0, "%u steps played after the stop beat", after);
    long long fade_end = (long long)last_sound - (long long)(beat - (beat % AJB_TICK));
    CHECK(fade_end >= AJB_FADE_SAMPLES - AJB_TICK && fade_end <= AJB_FADE_SAMPLES + AJB_TICK,
          "sound ended %lld samples after the stop beat, expected a %d-sample fade", fade_end, AJB_FADE_SAMPLES);
    CHECK(!ajb_clock_running(e), "clock should stop when both sides are silent");
    ajb_engine_destroy(e);
    free(ev.items);
}

/* ---- 6. clipping ---- */

static void test_clipping(void)
{
    printf("limiter: no output sample above -0.5 dBFS, even with 4x overdrive\n");
    const ajb_page_t *page = &ajb_factory_library.pages[0];
    int32_t worst = 0, worst_stress = 0;
    unsigned min_gain = 0xffffffffU;
    for (unsigned d = 0; d < AJB_SLOTS; ++d)
        for (unsigned m = 0; m < AJB_SLOTS; ++m) {
            if (!page->drums[d] || !page->melody[m]) continue;
            for (unsigned stress = 0; stress < 2; ++stress) {
                ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
                if (stress) { ajb_set_trim(e, 32768U * 4, 32768U * 4); ajb_set_bpm(e, 200); }
                ajb_press(e, AJB_DRUMS, d);
                ajb_press(e, AJB_MELODY, m);
                int32_t out[AJB_BLOCK * 2];
                for (unsigned b = 0; b < 4 * AJB_RATE / AJB_BLOCK; ++b) {
                    ajb_render(e, out);
                    for (unsigned i = 0; i < AJB_BLOCK * 2; ++i) {
                        int32_t a = abs(out[i]);
                        if (stress) worst_stress = a > worst_stress ? a : worst_stress;
                        else worst = a > worst ? a : worst;
                    }
                    if (stress && ajb_limiter_gain_q30() < min_gain) min_gain = ajb_limiter_gain_q30();
                }
                ajb_engine_destroy(e);
            }
        }
    CHECK(worst <= AJB_OUTPUT_CEILING, "normal mix peaked at %d (ceiling %d)", worst, AJB_OUTPUT_CEILING);
    CHECK(worst_stress <= AJB_OUTPUT_CEILING, "4x overdrive peaked at %d (ceiling %d)", worst_stress, AJB_OUTPUT_CEILING);
    CHECK(min_gain < 1073741824U, "4x overdrive never engaged the limiter");
    CHECK((worst + 128) >> 8 <= 32767 && (worst_stress + 128) >> 8 <= 32767, "16-bit WAV would clip");
    printf("  normal peak %.2f dBFS, overdrive peak %.2f dBFS, limiter down to %.1f dB\n",
           20 * log10(worst / 8388608.0), 20 * log10(worst_stress / 8388608.0),
           20 * log10(min_gain / 1073741824.0) - 6.02);
}

/* ---- 7. melody engines ---- */

/* Pitch by normalised autocorrelation over lags for 100 Hz to 2.6 kHz,
 * refined with a parabola through the peak. */
static double pitch_hz(const int32_t *x, unsigned n)
{
    unsigned best = 0;
    double best_r = 0;
    for (unsigned lag = 17; lag < 441; ++lag) {
        double r = 0, e = 0;
        for (unsigned i = 0; i + lag < n; ++i) { r += (double)x[i] * x[i + lag]; e += (double)x[i + lag] * x[i + lag]; }
        r = e > 0 ? r / sqrt(e) : 0;
        if (r > best_r * 1.02) { best_r = r; best = lag; }   /* first strong peak, not its multiples */
    }
    if (!best) return 0;
    double y[3];
    for (int k = -1; k <= 1; ++k) {
        double r = 0;
        for (unsigned i = 0; i + best + 1 < n; ++i) r += (double)x[i] * x[i + best + k];
        y[k + 1] = r;
    }
    double shift = 0.5 * (y[0] - y[2]) / (y[0] - 2 * y[1] + y[2]);
    return AJB_RATE / (best + shift);
}

static const ajb_instrument_t t_pluck = {.name = "Pluck", .kind = AJB_INSTRUMENT_PLUCK, .gain_q15 = 32768};
static const ajb_instrument_t t_psg = {.name = "PSG", .kind = AJB_INSTRUMENT_PSG, .gain_q15 = 32768};
static const ajb_instrument_t t_bass = {.name = "303", .kind = AJB_INSTRUMENT_BASS, .gain_q15 = 8192,
    .knobs = {{0, 127}, {2, 0}}, .knob_count = 2};   /* filter open, so the fundamental is clear */
static const ajb_melody_loop_t t_held[3] = {
    {.name = "Pluck A", .steps = 16, .instrument = &t_pluck, .octave = 0, .velocity = 90, .riff = "4---------------"},
    {.name = "PSG A", .steps = 16, .instrument = &t_psg, .octave = 0, .velocity = 90, .riff = "4---------------"},
    {.name = "303 A", .steps = 16, .instrument = &t_bass, .octave = 0, .velocity = 90, .riff = "4---------------"},
};
/* Degree 4 on every step: A4 in Happy, D5 in Dreamy, Bb4 in Spooky. */
static const ajb_melody_loop_t t_repeat = {.name = "PSG repeat", .steps = 16, .instrument = &t_psg, .octave = 0,
    .velocity = 90, .riff = "4444444444444444"};
static const ajb_page_t engines_page = {"Engines", {&kick_quarters},
    {&t_held[0], &t_held[1], &t_held[2], &t_repeat}, 0};
static const ajb_library_t engines_library = {.pages = &engines_page, .page_count = 1};

static void test_melody_engines(void)
{
    printf("melody engines: Pluck, PSG and 303 play in tune and stop silent\n");
    static const char *const names[] = {"Pluck", "PSG", "303"};
    const double a4 = 440.0;   /* degree 4 of octave 0 is A4 */
    for (unsigned k = 0; k < 3; ++k) {
        ajb_engine_t *e = ajb_engine_create(&engines_library);
        ajb_press(e, AJB_MELODY, k);
        enum { N = 8192 };
        static int32_t mono[N];
        int32_t out[AJB_BLOCK * 2];
        render_blocks(e, 16);                       /* skip the attack */
        for (unsigned b = 0; b < N / AJB_BLOCK; ++b) {
            ajb_render(e, out);
            for (unsigned i = 0; i < AJB_BLOCK; ++i) mono[b * AJB_BLOCK + i] = out[2 * i];
        }
        int32_t peak = 0;
        for (unsigned i = 0; i < N; ++i) peak = abs(mono[i]) > peak ? abs(mono[i]) : peak;
        double hz = pitch_hz(mono, N);
        CHECK(peak > AJB_OUTPUT_CEILING / 100, "%s is silent (peak %d)", names[k], peak);
        CHECK(fabs(hz / a4 - 1) < 0.01, "%s plays %.1f Hz, expected %.1f", names[k], hz, a4);
        printf("  %-5s A4 at %.1f Hz\n", names[k], hz);

        /* Same key again: silent from the end of the stop fade on. */
        ajb_press(e, AJB_MELODY, k);
        render_blocks(e, (AJB_RATE / 2) / AJB_BLOCK);
        CHECK(!ajb_clock_running(e), "%s: clock still running after stop", names[k]);
        CHECK(render_blocks(e, 64) == 0, "%s still sounds after the stop fade", names[k]);
        ajb_engine_destroy(e);
    }
}

static double crossings_hz(const int32_t *x, unsigned n);

/* Pitch of the left channel over the next n samples, by zero crossings
 * (the mood tests use the PSG square wave, which crosses once per cycle). */
static double capture_hz(ajb_engine_t *e, unsigned n)
{
    static int32_t mono[16384];
    int32_t out[AJB_BLOCK * 2];
    n = n > 16384 ? 16384 : n - n % AJB_BLOCK;
    for (unsigned b = 0; b < n / AJB_BLOCK; ++b) {
        ajb_render(e, out);
        for (unsigned i = 0; i < AJB_BLOCK; ++i) mono[b * AJB_BLOCK + i] = out[2 * i];
    }
    return crossings_hz(mono, n);
}

/* One held PSG note per scale degree, for checking every mood's notes. */
static const ajb_melody_loop_t t_degree[5] = {
    {.name = "deg 0", .steps = 16, .instrument = &t_psg, .velocity = 90, .riff = "0---------------"},
    {.name = "deg 1", .steps = 16, .instrument = &t_psg, .velocity = 90, .riff = "1---------------"},
    {.name = "deg 2", .steps = 16, .instrument = &t_psg, .velocity = 90, .riff = "2---------------"},
    {.name = "deg 3", .steps = 16, .instrument = &t_psg, .velocity = 90, .riff = "3---------------"},
    {.name = "deg 4", .steps = 16, .instrument = &t_psg, .velocity = 90, .riff = "4---------------"},
};
static const ajb_page_t degree_page = {"Degrees", {&kick_quarters},
    {&t_degree[0], &t_degree[1], &t_degree[2], &t_degree[3], &t_degree[4]}, 0};
static const ajb_library_t degree_library = {.pages = &degree_page, .page_count = 1};

static void test_mood_scales(void)
{
    printf("mood scales: every mood plays its PRD notes\n");
    /* PRD "Buttons and knobs", Mood, as MIDI notes from the C4 (60) octave. */
    static const struct { const char *name; int notes[5]; } spec[AJB_MOODS] = {
        {"Happy", {60, 62, 64, 67, 69}},  {"Dreamy", {65, 67, 69, 72, 74}},
        {"Spooky", {60, 63, 65, 67, 70}}, {"Magic", {60, 62, 64, 66, 68}},
        {"Floaty", {60, 62, 65, 67, 70}}, {"Faraway", {60, 62, 63, 67, 68}},
        {"Adventure", {60, 61, 64, 67, 68}}};
    unsigned wrong = 0;
    for (unsigned mood = 0; mood < AJB_MOODS; ++mood)
        for (unsigned d = 0; d < 5; ++d) {
            ajb_engine_t *e = ajb_engine_create(&degree_library);
            ajb_set_mood(e, mood);
            ajb_press(e, AJB_MELODY, d);
            render_blocks(e, 8);
            double hz = capture_hz(e, 8192), expect = 440.0 * pow(2.0, (spec[mood].notes[d] - 69) / 12.0);
            if (fabs(hz / expect - 1) > 0.01) {
                ++wrong;
                printf("  %s degree %u: %.1f Hz, expected %.1f\n", spec[mood].name, d, hz, expect);
            }
            ajb_engine_destroy(e);
        }
    CHECK(wrong == 0, "%u of %u mood notes out of tune", wrong, AJB_MOODS * 5U);
}

static void test_mood(void)
{
    printf("mood: changes land on the next bar line\n");
    ajb_engine_t *e = ajb_engine_create(&engines_library);
    ajb_set_mood(e, AJB_MOOD_SPOOKY);
    CHECK(ajb_mood(e) == AJB_MOOD_SPOOKY, "a stopped engine should take the mood at once");
    ajb_set_mood(e, AJB_MOOD_HAPPY);
    ajb_press(e, AJB_MELODY, 3);

    static const struct { unsigned mood; double hz; const char *name; } moods[] = {
        {AJB_MOOD_DREAMY, 587.33, "Dreamy D5"}, {AJB_MOOD_SPOOKY, 466.16, "Spooky Bb4"},
        {AJB_MOOD_HAPPY, 440.00, "Happy A4"}};
    unsigned previous = AJB_MOOD_HAPPY;
    double previous_hz = 440.0;
    for (unsigned k = 0; k < 3; ++k) {
        uint64_t bar = 16U * (k + 1);
        render_until(e, step_start(bar - 12, 110));
        ajb_set_mood(e, moods[k].mood);                    /* a quarter into the bar */
        CHECK(ajb_mood(e) == previous, "%s: mood changed before the bar line", moods[k].name);
        double before = capture_hz(e, 8192);               /* still inside the old bar */
        CHECK(fabs(before / previous_hz - 1) < 0.01, "%s: %.1f Hz before the bar, expected %.1f",
              moods[k].name, before, previous_hz);
        render_until(e, step_start(bar + 1, 110));
        CHECK(ajb_mood(e) == moods[k].mood, "%s: mood did not change on the bar line", moods[k].name);
        double after = capture_hz(e, 8192);
        CHECK(fabs(after / moods[k].hz - 1) < 0.01, "%s: %.1f Hz after the bar, expected %.1f",
              moods[k].name, after, moods[k].hz);
        printf("  %-10s %.1f Hz\n", moods[k].name, after);
        previous = moods[k].mood;
        previous_hz = moods[k].hz;
    }
    ajb_engine_destroy(e);
}

/* ---- 8. SEA5 ---- */

static uint8_t *sea5_encode(const int16_t *pcm, unsigned frames) { return fm1_sea_encode(pcm, frames, ajb_sea5_dqt()); }

static void test_sea5(void)
{
    printf("sea5: round trip quality, and any frame decodes like a straight run\n");
    enum { N = 32000 };
    static int16_t pcm[N];
    uint32_t rng = 7;
    for (unsigned i = 0; i < N; ++i) {
        rng = rng * 1664525U + 1013904223U;
        double t = i / 32000.0;
        pcm[i] = (int16_t)(12000 * sin(2 * M_PI * 440 * t) + 5000 * sin(2 * M_PI * 1250 * t) + (int)(rng >> 22) - 512);
    }
    uint8_t *data = sea5_encode(pcm, N);
    const ajb_sample_t s = {"test", data, N, 32000, 60, 16};
    static int32_t straight[N];
    ajb_voice_t v;
    ajb_voice_reset(&v);
    v.sample = &s;
    double signal = 0, noise = 0;
    for (unsigned i = 0; i < N; ++i) {
        straight[i] = ajb_voice_source(&v, i);
        signal += (double)pcm[i] * pcm[i];
        noise += (double)(pcm[i] - straight[i]) * (pcm[i] - straight[i]);
    }
    unsigned seek_bad = 0;
    for (unsigned k = 0; k < 200; ++k) {
        rng = rng * 1664525U + 1013904223U;
        uint32_t frame = (rng >> 8) % N;
        ajb_voice_reset(&v);
        v.sample = &s;
        seek_bad += ajb_voice_source(&v, frame) != straight[frame];
    }
    double snr = 10 * log10(signal / noise);
    CHECK(snr > 20.0, "SNR %.1f dB", snr);
    CHECK(seek_bad == 0, "%u of 200 random frames decode differently from a straight run", seek_bad);
    printf("  SNR %.1f dB at 5.7 bits per sample\n", snr);
    free(data);
}

/* ---- 9. sampler pitch and slices ---- */

static double crossings_hz(const int32_t *x, unsigned n)
{
    unsigned c = 0, first = 0, last = 0;
    for (unsigned i = 1; i < n; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (!c) first = i; last = i; ++c; }
    return c > 1 ? (c - 1) * (double)AJB_RATE / (last - first) : 0;
}

static void test_sampler(void)
{
    printf("sampler: notes follow pitch, slices keep pitch and length\n");
    enum { N = 16000 };
    static int16_t pcm[N];
    for (unsigned i = 0; i < N; ++i) pcm[i] = (int16_t)(16000 * sin(2 * M_PI * 500.0 * i / 32000.0));
    uint8_t *data = sea5_encode(pcm, N);
    const ajb_sample_t note = {"sine", data, N, 32000, 60, 0};
    const ajb_sample_t loop = {"sine loop", data, N, 32000, 60, 16};
    static const int offsets[] = {-12, 0, 7, 12};
    for (unsigned k = 0; k < 4; ++k) {
        ajb_voice_t v;
        ajb_voice_reset(&v);
        ajb_voice_note(&v, &note, 60 + offsets[k], 32768);
        static int32_t bus[20000];
        memset(bus, 0, sizeof bus);
        ajb_voice_render(&v, bus, 20000);
        double expect = 500.0 * pow(2.0, offsets[k] / 12.0);
        double hz = crossings_hz(bus + 500, 4000);
        CHECK(fabs(hz / expect - 1) < 0.01, "note %+d: %.1f Hz, expected %.1f", offsets[k], hz, expect);
    }
    ajb_voice_t v;
    ajb_voice_reset(&v);
    ajb_voice_slice(&v, &loop, 3, 32768);
    static int32_t bus[4000];
    memset(bus, 0, sizeof bus);
    ajb_voice_render(&v, bus, 4000);
    unsigned last = 0;
    for (unsigned i = 0; i < 4000; ++i) if (bus[i]) last = i;
    unsigned expect = 1000U * AJB_RATE / 32000U;   /* 1000 source frames per slice */
    CHECK(last + 40 >= expect && last <= expect + 40, "slice lasted %u samples, expected about %u", last, expect);
    CHECK(fabs(crossings_hz(bus, expect) / 500.0 - 1) < 0.02, "slice pitch moved");
    free(data);
}

int main(void)
{
    test_clock();
    test_engine_timing();
    test_onsets();
    test_swap_step();
    test_stop();
    test_clipping();
    test_melody_engines();
    test_mood();
    test_mood_scales();
    test_sea5();
    test_sampler();
    test_s2();
    test_library();
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
