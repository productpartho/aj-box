/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Desktop tests for the S2 controls (PRD "Control map", "Effects",
 * "Playback rules"): paging, Dice, Fill, Fast, Mute, Finish, PLAY/STOP,
 * HOME, the 6 effects and Swoosh, sound swaps, octave, swap timing, hearts,
 * jams, power-off memory and the start-up chime. */
#include "support.h"
#include <math.h>
#include <string.h>

static const int32_t FS = 8388608;   /* Q23 full scale */

/* ---- a small two-page library with easy-to-measure sounds ---- */

/* Quiet enough that the limiter never works, so renders compare exactly. */
enum { DRUM_GAIN = 8192 };
static const ajb_drum_loop_t kick4 = {"Kick", 16, NULL, {[AJB_BD] = "X...X...X...X..."}, DRUM_GAIN};
static const ajb_drum_loop_t kick8 = {"Kick 8", 16, NULL, {[AJB_BD] = "X.X.X.X.X.X.X.X."}, DRUM_GAIN};
static const ajb_drum_loop_t rims = {"Rim 16ths", 16, NULL, {[AJB_RS] = "XXXXXXXXXXXXXXXX"}, DRUM_GAIN};
static const ajb_instrument_t psg_dry = {.name = "PSG", .kind = AJB_INSTRUMENT_PSG, .gain_q15 = 16384,
                                         .knobs = {{3, 0}}, .knob_count = 1};
static const ajb_instrument_t pluck = {.name = "Pluck", .kind = AJB_INSTRUMENT_PLUCK, .gain_q15 = 32768};
static const ajb_melody_loop_t eighths = {.name = "Eighths", .steps = 16, .instrument = &psg_dry,
                                          .velocity = 100, .riff = "4.4.4.4.4.4.4.4."};
static const ajb_melody_loop_t held = {.name = "Held", .steps = 16, .instrument = &psg_dry,
                                       .velocity = 100, .riff = "4---------------"};
static const ajb_melody_loop_t plucks = {.name = "Plucks", .steps = 16, .instrument = &pluck,
                                         .velocity = 100, .riff = "0...2...3...4..."};
static const ajb_kit_edit_t kit_b_edits[] = {{AJB_BD, 3, 100}};
static const ajb_kit_t kit_a = {"A", NULL, 0}, kit_b = {"B", kit_b_edits, 1};
static const ajb_kit_t *const kits[] = {&kit_a, &kit_b};
static const ajb_instrument_t *const instruments[] = {&psg_dry, &pluck};
static const ajb_page_t pages[] = {
    {"Zero", {&kick4, &kick8}, {&eighths, &held}, 0},
    {"One", {&kick8}, {&plucks}, 1},
};
static const ajb_library_t lib = {.pages = pages, .page_count = 2, .kits = kits, .kit_count = 2,
                                  .instruments = instruments, .instrument_count = 2, .fill = &rims};

enum { BPM = 110, MAXN = 44100 * 8 };
static int32_t buf_a[MAXN], buf_b[MAXN];

static double rms(const int32_t *x, size_t n)
{
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += (double)x[i] * x[i];
    return n ? sqrt(s / n) : 0;
}
static int32_t peak_of(const int32_t *x, size_t n)
{
    int32_t p = 0;
    for (size_t i = 0; i < n; ++i) p = abs(x[i]) > p ? abs(x[i]) : p;
    return p;
}
/* RMS above about 3 kHz: two one-pole high-passes in a row. */
static double treble(const int32_t *x, size_t n)
{
    const double k = 1.0 - exp(-2.0 * 3.14159265358979 * 3000.0 / AJB_RATE);
    double low1 = 0, low2 = 0, s = 0;
    for (size_t i = 0; i < n; ++i) {
        low1 += k * (x[i] - low1);
        double high = x[i] - low1;
        low2 += k * (high - low2);
        high -= low2;
        s += high * high;
    }
    return n ? sqrt(s / n) : 0;
}
/* Sharp attacks: armed above half the peak, re-armed after 500 samples
 * below a fifth of it. */
static unsigned onsets(const int32_t *x, size_t n)
{
    unsigned count = 0, quiet = 0;
    int armed = 1;
    int32_t peak = peak_of(x, n);
    for (size_t i = 0; i < n; ++i) {
        int32_t a = abs(x[i]);
        if (a > peak / 2 && armed) { ++count; armed = 0; }
        quiet = a < peak / 5 ? quiet + 1 : 0;
        if (quiet >= 500) armed = 1;
    }
    return count;
}
static ajb_engine_t *engine(void)
{
    ajb_engine_t *e = ajb_engine_create(&lib);
    ajb_set_bpm(e, BPM);
    return e;
}

/* ---- paging and Dice ---- */

static void test_paging_dice(void)
{
    printf("s2 paging: Back/Next change the keys' page, the playing loop carries on\n");
    ajb_engine_t *e = engine();
    ajb_status_t st;
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(2, BPM));
    ajb_page_step(e, AJB_DRUMS, +1);
    ajb_status(e, &st);
    CHECK(st.view_page[AJB_DRUMS] == 1 && st.slot[AJB_DRUMS] == 0 && st.page[AJB_DRUMS] == 0,
          "Next should show page 1 and leave page 0's loop playing");
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(4, BPM));
    ajb_status(e, &st);
    CHECK(st.page[AJB_DRUMS] == 1 && st.slot[AJB_DRUMS] == 0, "key 1 on page 1 should play page 1's loop");
    ajb_page_step(e, AJB_DRUMS, +1);
    ajb_status(e, &st);
    CHECK(st.view_page[AJB_DRUMS] == 0, "pages should wrap from the last to the first");
    ajb_page_step(e, AJB_DRUMS, -1);
    ajb_status(e, &st);
    CHECK(st.view_page[AJB_DRUMS] == 1, "Back should wrap from the first to the last");
    ajb_engine_destroy(e);

    printf("s2 dice: a surprise loop from any page, never the playing one\n");
    e = engine();
    events_t ev = {0};
    ajb_set_event_hook(e, capture, &ev);
    ajb_press(e, AJB_MELODY, 0);
    unsigned repeats = 0, pages_seen = 0, dice_events = 0;
    for (unsigned k = 0; k < 40; ++k) {
        render_blocks(e, 50);
        ajb_status(e, &st);
        int before_page = st.page[AJB_MELODY], before_slot = st.slot[AJB_MELODY];
        ajb_dice(e, AJB_MELODY);
        render_until(e, ajb_now(e) + 6100);
        ajb_status(e, &st);
        repeats += st.page[AJB_MELODY] == before_page && st.slot[AJB_MELODY] == before_slot;
        pages_seen |= 1U << st.page[AJB_MELODY];
        CHECK(st.view_page[AJB_MELODY] == st.page[AJB_MELODY], "keys should show the Dice loop's page");
    }
    for (size_t i = 0; i < ev.count; ++i) dice_events += ev.items[i].kind == AJB_EVENT_DICE;
    CHECK(repeats == 0, "Dice repeated the playing loop %u times", repeats);
    CHECK(pages_seen == 3, "Dice should reach both pages");
    CHECK(dice_events == 40, "%u Dice events for 40 rolls", dice_events);
    free(ev.items);
    ajb_engine_destroy(e);
}

/* ---- Fill, Fast and Mute ---- */

static void test_holds(void)
{
    printf("s2 fill: held, the drums play the fill; released, the loop comes back\n");
    ajb_engine_t *e = engine();
    ajb_press(e, AJB_DRUMS, 0);
    /* Press and release just before the bar line, so each bar is whole. */
    size_t n = render_capture(e, step_start(16, BPM) - 2 * AJB_BLOCK, buf_a, MAXN);
    unsigned plain = onsets(buf_a, n);
    ajb_hold(e, AJB_HOLD_FILL, 1);
    n = render_capture(e, step_start(32, BPM) - 2 * AJB_BLOCK, buf_a, MAXN);
    unsigned filled = onsets(buf_a, n);
    ajb_hold(e, AJB_HOLD_FILL, 0);
    n = render_capture(e, step_start(48, BPM) - 2 * AJB_BLOCK, buf_a, MAXN);
    unsigned after = onsets(buf_a, n);
    CHECK(plain == 4 && after == 4, "kick loop: %u then %u hits per bar, expected 4", plain, after);
    CHECK(filled == 16, "fill bar: %u hits, expected the 16 rim shots", filled);
    ajb_engine_destroy(e);

    printf("s2 fast: held, melody notes come twice as fast, on the half steps\n");
    e = engine();
    events_t ev = {0};
    ajb_set_event_hook(e, capture, &ev);
    ajb_press(e, AJB_MELODY, 0);
    render_until(e, step_start(16, BPM) - 100);
    ajb_hold(e, AJB_HOLD_FAST, 1);
    render_until(e, step_start(32, BPM) - 100);
    ajb_hold(e, AJB_HOLD_FAST, 0);
    render_until(e, step_start(48, BPM) - 100);
    unsigned bars[3] = {0}, off_grid = 0;
    for (size_t i = 0; i < ev.count; ++i) {
        const ajb_event_t *x = &ev.items[i];
        if (x->kind != AJB_EVENT_STEP || x->side != AJB_MELODY) continue;
        unsigned bar = (unsigned)(x->global_step / 16U);
        if (bar < 3) ++bars[bar];
        uint64_t s = step_start(x->global_step, BPM), mid = (x->global_step * AJB_STEP_PHASE + AJB_STEP_PHASE / 2 + BPM - 1) / BPM;
        uint64_t target = x->sample + AJB_TICK > mid && x->sample <= mid ? mid : s;
        if (x->sample > target || target - x->sample >= AJB_TICK) ++off_grid;
    }
    CHECK(bars[0] == 16 && bars[1] == 32 && bars[2] == 16, "melody steps per bar %u, %u, %u; expected 16, 32, 16",
          bars[0], bars[1], bars[2]);
    CHECK(off_grid == 0, "%u fast notes off the step and half-step grid", off_grid);
    free(ev.items);
    ajb_engine_destroy(e);

    printf("s2 mute: held, that side drops out; released, it returns on the beat\n");
    e = engine();
    ajb_press(e, AJB_DRUMS, 0);
    ajb_press(e, AJB_MELODY, 0);
    render_until(e, step_start(17, BPM));
    ajb_hold(e, AJB_HOLD_MUTE_DRUMS, 1);
    render_until(e, step_start(17, BPM) + 1000);
    ajb_take_side_peak(e, AJB_DRUMS);
    ajb_take_side_peak(e, AJB_MELODY);
    render_until(e, step_start(22, BPM) + 1000);
    ajb_hold(e, AJB_HOLD_MUTE_DRUMS, 0);
    render_until(e, step_start(24, BPM) - 2 * AJB_BLOCK);
    uint32_t muted = ajb_take_side_peak(e, AJB_DRUMS), melody = ajb_take_side_peak(e, AJB_MELODY);
    render_until(e, step_start(24, BPM) + 2000);
    uint32_t back = ajb_take_side_peak(e, AJB_DRUMS);
    CHECK(muted == 0, "muted drums still peaked at %u", muted);
    CHECK(melody > 0, "melody should keep playing while drums are muted");
    CHECK(back > 0, "drums should come back on the beat after release");
    ajb_engine_destroy(e);
}

/* ---- Finish, PLAY/STOP and HOME ---- */

static void test_finish_stop_home(void)
{
    printf("s2 finish: on the next bar a crash and final chord, then stop\n");
    ajb_engine_t *e = engine();
    events_t ev = {0};
    ajb_set_event_hook(e, capture, &ev);
    ajb_press(e, AJB_DRUMS, 0);
    ajb_press(e, AJB_MELODY, 0);
    render_until(e, step_start(5, BPM));
    ajb_finish(e);
    size_t n = render_capture(e, step_start(16, BPM) + AJB_RATE, buf_a, MAXN);
    uint64_t finish_at = UINT64_MAX;
    unsigned late_steps = 0;
    for (size_t i = 0; i < ev.count; ++i) {
        if (ev.items[i].kind == AJB_EVENT_FINISH) finish_at = ev.items[i].sample;
        if (ev.items[i].kind == AJB_EVENT_STEP && ev.items[i].global_step >= 16) ++late_steps;
    }
    CHECK(finish_at <= step_start(16, BPM) && step_start(16, BPM) - finish_at < AJB_TICK,
          "Finish landed at %llu, expected the bar at %llu", (unsigned long long)finish_at,
          (unsigned long long)step_start(16, BPM));
    CHECK(late_steps == 0, "%u loop steps played after Finish", late_steps);
    size_t bar = (size_t)step_start(16, BPM);
    CHECK(n > bar + 4410 && peak_of(buf_a + bar, 4410) > FS / 20, "no crash and chord after Finish");
    render_until(e, step_start(16 + 40, BPM));
    CHECK(!ajb_clock_running(e), "the clock should stop after the Finish rings out");
    free(ev.items);
    ajb_engine_destroy(e);

    printf("s2 play/stop: stops both at once; again resumes the last pair\n");
    e = engine();
    ajb_press(e, AJB_DRUMS, 1);
    ajb_press(e, AJB_MELODY, 1);
    render_until(e, step_start(6, BPM) + 100);
    ajb_play_stop(e);
    uint64_t stop = ajb_now(e);
    render_until(e, stop + AJB_FADE_SAMPLES + 2 * AJB_TICK);
    n = render_capture(e, stop + AJB_RATE / 2, buf_a, MAXN);
    ajb_status_t st;
    ajb_status(e, &st);
    CHECK(peak_of(buf_a, n) == 0, "sound after PLAY/STOP: peak %d", peak_of(buf_a, n));
    CHECK(st.slot[0] == -1 && st.slot[1] == -1 && !st.running, "both sides and the clock should stop");
    ajb_play_stop(e);
    render_blocks(e, 4);
    ajb_status(e, &st);
    CHECK(st.slot[0] == 1 && st.slot[1] == 1 && st.running, "PLAY/STOP again should resume the last pair");
    ajb_engine_destroy(e);

    printf("s2 home: every effect off, Swoosh back to the middle\n");
    e = engine();
    ajb_effect(e, AJB_FX_ECHO, 1);
    ajb_effect(e, AJB_FX_WOBBLE, 1);
    ajb_knob(e, AJB_KNOB_SWOOSH, 0);
    ajb_home(e);
    ajb_status(e, &st);
    CHECK(st.effects == 0 && st.knob[AJB_KNOB_SWOOSH] == 64, "HOME left effects 0x%x, Swoosh %u", st.effects,
          st.knob[AJB_KNOB_SWOOSH]);
    ajb_engine_destroy(e);
}

/* ---- effects ---- */

/* Two bars of drums + melody from bar 2, with `setup` applied at bar 1. */
static size_t effect_take(int32_t *out, int effect, unsigned amount, unsigned speed, unsigned swoosh)
{
    ajb_engine_t *e = engine();
    ajb_press(e, AJB_DRUMS, 0);
    ajb_press(e, AJB_MELODY, 0);
    render_until(e, step_start(16, BPM));
    if (effect >= 0) {
        ajb_effect(e, (unsigned)effect, 1);
        ajb_knob(e, AJB_KNOB_AMOUNT, amount);
        ajb_knob(e, AJB_KNOB_SPEED, speed);
    }
    ajb_knob(e, AJB_KNOB_SWOOSH, swoosh);
    size_t n = render_capture(e, step_start(48, BPM), out, MAXN);
    ajb_engine_destroy(e);
    return n;
}

static void test_effects(void)
{
    printf("s2 effects: each one changes the sound, none goes over the ceiling\n");
    static const char *const names[AJB_EFFECTS] = {"Echo", "Sparkle", "Wobble", "Wah", "Crunch", "Stutter"};
    size_t n = effect_take(buf_b, -1, 0, 0, 64);
    double dry = rms(buf_b, n);
    for (unsigned k = 0; k < AJB_FX_STUTTER; ++k) {
        size_t m = effect_take(buf_a, (int)k, 127, 64, 64);
        double diff = 0;
        for (size_t i = 0; i < m && i < n; ++i) diff += fabs((double)buf_a[i] - buf_b[i]);
        diff /= (double)m;
        CHECK(diff > dry * 0.02, "%s barely changes the sound (%.0f vs rms %.0f)", names[k], diff, dry);
        CHECK(peak_of(buf_a, m) <= AJB_OUTPUT_CEILING, "%s went over the ceiling", names[k]);
        if (k == AJB_FX_CRUNCH)
            CHECK(rms(buf_a, m) <= dry * 1.122, "Crunch got louder: %.2f dB", 20 * log10(rms(buf_a, m) / dry));
    }

    printf("s2 echo: feedback capped, so the echoes always die away\n");
    ajb_engine_t *e = engine();
    ajb_press(e, AJB_DRUMS, 0);
    ajb_effect(e, AJB_FX_ECHO, 1);
    ajb_knob(e, AJB_KNOB_AMOUNT, 127);
    render_until(e, step_start(16, BPM));
    ajb_play_stop(e);
    size_t m = render_capture(e, step_start(16, BPM) + 6 * AJB_RATE, buf_a, MAXN);
    double first = rms(buf_a, AJB_RATE / 2), last = rms(buf_a + m - AJB_RATE / 2, AJB_RATE / 2);
    CHECK(first > 0 && last < first * 0.05, "echo tail %.0f after 6 s against %.0f at the start", last, first);
    ajb_engine_destroy(e);

    printf("s2 stutter: held, the last 1/8 note repeats; released, the loop carries on\n");
    /* One engine at a time: FM1 Open's limiter and delay lines are global. */
    uint64_t press = step_start(21, BPM) + 1500;
    uint32_t length = AJB_RATE * 60U / BPM / 2U;      /* a 1/8 note */
    ajb_engine_t *b = engine();
    ajb_press(b, AJB_DRUMS, 0); ajb_press(b, AJB_MELODY, 0);
    render_until(b, press);
    render_until(b, press + 4 * length);
    uint64_t release = ajb_now(b);
    render_until(b, release + 1024);
    size_t mb = render_capture(b, release + AJB_RATE, buf_b, MAXN);
    ajb_engine_destroy(b);

    ajb_engine_t *a = engine();
    ajb_press(a, AJB_DRUMS, 0); ajb_press(a, AJB_MELODY, 0);
    render_until(a, press);
    ajb_hold(a, AJB_HOLD_STUTTER, 1);
    m = render_capture(a, press + 4 * length, buf_a, MAXN);
    double c = 0, e0 = 0, e1 = 0;
    for (size_t i = 1024; i + length < m; ++i) {
        c += (double)buf_a[i] * buf_a[i + length];
        e0 += (double)buf_a[i] * buf_a[i];
        e1 += (double)buf_a[i + length] * buf_a[i + length];
    }
    double correlation = c / sqrt(e0 * e1);
    CHECK(correlation > 0.95, "stutter repeats correlate only %.3f at one 1/8 note", correlation);
    ajb_hold(a, AJB_HOLD_STUTTER, 0);
    CHECK(ajb_now(a) == release, "the two takes should line up");
    render_until(a, release + 1024);
    size_t ma = render_capture(a, release + AJB_RATE, buf_a, MAXN);
    unsigned differ = 0;
    for (size_t i = 0; i < ma && i < mb; ++i) differ += buf_a[i] != buf_b[i];
    CHECK(differ == 0, "after release, %u samples differ from the loop that never stuttered", differ);
    ajb_engine_destroy(a);

    printf("s2 swoosh: the middle is clean; left is underwater, right a tiny radio\n");
    n = effect_take(buf_b, -1, 0, 0, 64);
    unsigned differ_clean = 0;
    for (unsigned v = 63; v <= 65; ++v) {
        m = effect_take(buf_a, -1, 0, 0, v);
        for (size_t i = 0; i < m && i < n; ++i) differ_clean += buf_a[i] != buf_b[i];
    }
    CHECK(differ_clean == 0, "the clean zone changed %u samples", differ_clean);
    double hf_dry = treble(buf_b, n), lf_dry = 0, lf = 0, avg = 0;
    for (size_t i = 1; i < n; ++i) {
        avg += ((double)buf_b[i] - avg) * 0.01;
        lf_dry += fabs(avg);
    }
    m = effect_take(buf_a, -1, 0, 0, 0);
    double hf = treble(buf_a, m);
    CHECK(hf < hf_dry * 0.25, "Swoosh left kept %.0f%% of the treble", 100 * hf / hf_dry);
    m = effect_take(buf_a, -1, 0, 0, 127);
    avg = 0;
    for (size_t i = 1; i < m; ++i) { avg += ((double)buf_a[i] - avg) * 0.01; lf += fabs(avg); }
    CHECK(lf < lf_dry * 0.35, "Swoosh right kept %.0f%% of the bass", 100 * lf / lf_dry);
}

/* ---- sound swaps, octave and swap timing ---- */

static void test_swaps(void)
{
    printf("s2 kits: PRESETS lands on the next drum step; another page brings its own kit\n");
    ajb_engine_t *e = engine();
    ajb_status_t st;
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(3, BPM) + 1000);
    ajb_set_kit(e, 1);
    ajb_status(e, &st);
    unsigned before = st.kit;
    render_until(e, step_start(4, BPM) + AJB_TICK);
    ajb_status(e, &st);
    CHECK(before == 0 && st.kit == 1, "kit %u before the next step, %u after; expected 0 then 1", before, st.kit);
    ajb_set_kit(e, 0);
    ajb_page_step(e, AJB_DRUMS, 1);
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(8, BPM) + AJB_TICK);
    ajb_status(e, &st);
    CHECK(st.kit == 1, "page 1's drums should bring page 1's kit back, got kit %u", st.kit);
    ajb_engine_destroy(e);

    printf("s2 instruments: ALGORITHM waits for the next note\n");
    e = engine();
    ajb_press(e, AJB_MELODY, 1);                          /* one held PSG note per bar */
    render_until(e, step_start(17, BPM));
    size_t n = render_capture(e, step_start(20, BPM), buf_a, MAXN);
    double before_swap = rms(buf_a, n);
    ajb_set_instrument(e, 1);                             /* Pluck */
    n = render_capture(e, step_start(30, BPM), buf_a, MAXN);
    double held_on = rms(buf_a, n);
    render_until(e, step_start(32, BPM));
    n = render_capture(e, step_start(34, BPM), buf_a, MAXN);
    double attack = rms(buf_a, n);
    render_until(e, step_start(44, BPM));
    n = render_capture(e, step_start(47, BPM), buf_a, MAXN);
    double tail = rms(buf_a, n);
    CHECK(fabs(held_on / before_swap - 1) < 0.1, "the sounding note changed level at the swap (%.2f)", held_on / before_swap);
    CHECK(attack > 0 && tail < attack * 0.5, "the next note should be a plucked, decaying string");
    ajb_engine_destroy(e);

    printf("s2 octave: OCT moves the melody, clamped to two octaves\n");
    e = engine();
    ajb_set_octave(e, 1);
    ajb_press(e, AJB_MELODY, 1);
    render_until(e, 4096);
    n = render_capture(e, 4096 + 16384, buf_a, MAXN);
    unsigned crossings = 0;
    size_t first = 0, last = 0;
    for (size_t i = 1; i < n; ++i)
        if (buf_a[i - 1] < 0 && buf_a[i] >= 0) { if (!crossings) first = i; last = i; ++crossings; }
    double hz = crossings > 1 ? (crossings - 1) * (double)AJB_RATE / (double)(last - first) : 0;
    CHECK(fabs(hz / 880.0 - 1) < 0.01, "OCT+ played %.1f Hz, expected A5 880", hz);
    ajb_set_octave(e, 5);
    ajb_status(e, &st);
    CHECK(st.octave == 2, "octave should clamp at +2, got %d", st.octave);
    ajb_engine_destroy(e);

    printf("s2 swap timing: parent mode 'next bar' waits for the bar line\n");
    e = engine();
    events_t ev = {0};
    ajb_set_event_hook(e, capture, &ev);
    ajb_set_swap_mode(e, AJB_SWAP_BAR);
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, step_start(5, BPM) + 300);
    ajb_press(e, AJB_DRUMS, 1);
    render_until(e, step_start(20, BPM));
    const ajb_event_t *swap = first_of(&ev, AJB_DRUMS, 1, 0);
    CHECK(swap && swap->global_step == 16, "next-bar swap joined at step %llu",
          swap ? (unsigned long long)swap->global_step : 0ULL);
    free(ev.items);
    ajb_engine_destroy(e);
}

/* ---- hearts, jams, memory and the chime ---- */

static void test_memory(void)
{
    printf("s2 hearts: SAVE keeps the moment, SEQ brings it back, 8 at most\n");
    ajb_engine_t *e = engine();
    ajb_status_t st;
    ajb_press(e, AJB_DRUMS, 0);
    ajb_press(e, AJB_MELODY, 0);
    ajb_effect(e, AJB_FX_ECHO, 1);
    ajb_set_bpm(e, 120);
    render_blocks(e, 100);
    CHECK(ajb_save_heart(e) == 1, "the first heart should be number 1");
    ajb_page_step(e, AJB_DRUMS, 1);
    ajb_press(e, AJB_DRUMS, 0);
    ajb_press(e, AJB_MELODY, 1);
    ajb_effect(e, AJB_FX_ECHO, 0);
    ajb_set_bpm(e, 100);
    ajb_set_mood(e, AJB_MOOD_SPOOKY);
    render_blocks(e, 600);
    CHECK(ajb_save_heart(e) == 2, "the second heart should be number 2");
    CHECK(ajb_next_heart(e) == 1, "SEQ from heart 2 should go to heart 1");
    render_blocks(e, 600);
    ajb_status(e, &st);
    CHECK(st.page[0] == 0 && st.slot[0] == 0 && st.slot[1] == 0 && st.bpm == 120 &&
          st.effects == (1U << AJB_FX_ECHO) && st.mood == AJB_MOOD_HAPPY, "heart 1 did not come back whole");
    for (unsigned k = 0; k < 7; ++k) ajb_save_heart(e);
    const ajb_hearts_t *h = ajb_hearts(e);
    CHECK(h->count == 8 && h->newest == 0, "9 saves should keep 8 hearts, the newest replacing the oldest");
    ajb_engine_destroy(e);

    printf("s2 jam: REC keeps every control; ARP plays it back the same\n");
    e = engine();
    events_t first = {0}, second = {0};
    ajb_set_event_hook(e, capture, &first);
    ajb_record(e);
    uint64_t t0 = ajb_now(e);
    ajb_press(e, AJB_DRUMS, 0);
    render_until(e, t0 + 30000);
    ajb_press(e, AJB_MELODY, 0);
    render_until(e, t0 + 70000);
    ajb_knob(e, AJB_KNOB_BALANCE, 90);
    ajb_hold(e, AJB_HOLD_FAST, 1);
    render_until(e, t0 + 100000);
    ajb_hold(e, AJB_HOLD_FAST, 0);
    ajb_press(e, AJB_DRUMS, 1);
    ajb_dice(e, AJB_MELODY);
    render_until(e, t0 + 160000);
    ajb_set_mood(e, AJB_MOOD_MAGIC);
    render_until(e, t0 + 220000);
    ajb_record(e);
    size_t n1 = first.count;
    ajb_play_stop(e);
    render_blocks(e, 400);
    ajb_set_event_hook(e, capture, &second);
    ajb_replay(e);
    uint64_t t1 = ajb_now(e);
    ajb_status(e, &st);
    CHECK(st.replaying, "ARP should start the replay");
    render_until(e, t1 + 230000);
    unsigned matched = 0, steps1 = 0, ended = 0;
    size_t j = 0;
    for (size_t i = 0; i < n1; ++i) {
        const ajb_event_t *x = &first.items[i];
        if (x->kind != AJB_EVENT_STEP) continue;
        ++steps1;
        while (j < second.count && second.items[j].kind != AJB_EVENT_STEP) ++j;
        if (j < second.count && second.items[j].sample - t1 == x->sample - t0 && second.items[j].side == x->side &&
            second.items[j].slot == x->slot && second.items[j].page == x->page &&
            second.items[j].loop_step == x->loop_step) ++matched;
        ++j;
    }
    for (size_t i = 0; i < second.count; ++i) ended += second.items[i].kind == AJB_EVENT_JAM_END;
    CHECK(steps1 > 50 && matched == steps1, "replay matched %u of %u recorded steps", matched, steps1);
    CHECK(ended == 1, "the replay should end once");
    free(first.items);
    free(second.items);
    ajb_engine_destroy(e);

    printf("s2 memory: power-off snapshot restores sounds, and PLAY/STOP resumes the pair\n");
    ajb_engine_t *a = engine(), *b = engine();
    ajb_page_step(a, AJB_MELODY, 1);
    ajb_press(a, AJB_DRUMS, 1);
    ajb_press(a, AJB_MELODY, 0);
    ajb_set_kit(a, 1);
    ajb_effect(a, AJB_FX_SPARKLE, 1);
    ajb_knob(a, AJB_KNOB_AMOUNT, 100);
    ajb_set_bpm(a, 96);
    ajb_set_octave(a, -1);
    ajb_set_mood(a, AJB_MOOD_FLOATY);
    render_blocks(a, 200);
    ajb_snapshot_t sa, sb;
    ajb_snapshot(a, &sa);
    ajb_restore(b, &sa);
    ajb_snapshot(b, &sb);
    sa.drum_slot = sb.drum_slot = sa.melody_slot = sb.melody_slot = 0;
    sa.drum_page = sb.drum_page = sa.melody_page = sb.melody_page = 0;
    sa.clock_step = sb.clock_step = 0;
    sa.clock_offset = sb.clock_offset = 0;
    CHECK(memcmp(&sa, &sb, sizeof sa) == 0, "restored sounds, effects, tempo or pages differ");
    ajb_status(b, &st);
    CHECK(!st.running, "restore should not start playing by itself");
    ajb_play_stop(b);
    render_blocks(b, 4);
    ajb_status(b, &st);
    CHECK(st.slot[0] == 1 && st.page[1] == 1 && st.slot[1] == 0, "PLAY/STOP after restore should resume the pair");
    ajb_engine_destroy(a);
    ajb_engine_destroy(b);

    printf("s2 chime: a quiet hello, about a second long\n");
    e = engine();
    ajb_chime(e);
    size_t n = render_capture(e, AJB_RATE * 2, buf_a, MAXN);
    int32_t loud = peak_of(buf_a, n), late = peak_of(buf_a + AJB_RATE * 3 / 2, n - AJB_RATE * 3 / 2);
    CHECK(loud > FS / 100 && loud < FS / 4, "chime peak %.1f dBFS, expected quiet but audible", 20 * log10((double)loud / FS));
    CHECK(late < loud / 4, "chime still at %.1f dBFS after 1.5 s", 20 * log10((double)late / FS + 1e-9));
    ajb_engine_destroy(e);
}

void test_s2(void)
{
    test_paging_dice();
    test_holds();
    test_finish_stop_home();
    test_effects();
    test_swaps();
    test_memory();
}
