/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Tests for AJ Box's app logic on FM1 Open's stock instrument (ajapp.c),
 * run in FM1 Open's simulator: PRD "Playback rules" and "Control map". */
#define main ajstock_main
#include "ajstock.c"
#undef main

static unsigned checks, failures;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

enum { T_DRUMS, T_SAMPLER, T_FM, T_PSG, T_PLUCK, T_BASS };
static const unsigned STEP_BLOCKS = 47;     /* a 16th at 110 BPM is 6,014 samples: 47 blocks */

static ajapp_status_t now(void) { ajapp_status_t st; ajapp_status(&st); return st; }
static unsigned notes_in(unsigned track)
{
    struct groove_step cells[GROOVE_STEPS];
    unsigned n = 0;
    groove_sequence_read(track, 0, cells, GROOVE_STEPS);
    for (unsigned s = 0; s < GROOVE_STEPS; ++s) n += cells[s].velocity != 0;
    return n;
}
static void fresh(void) { start(NULL); }
/* Run until the 4-bar cycle is on `step`, at most 5 bars. */
static void run_to_step(unsigned step)
{
    for (unsigned b = 0; b < 5 * 16 * STEP_BLOCKS && now().step != step; ++b) block();
}

static void test_keys(void)
{
    printf("keys: a press starts the beat, a swap keeps its place, the same key stops on the next beat\n");
    fresh();
    CHECK(!now().running, "running before any key");
    ajapp_press(AJAPP_DRUMS, 0);
    ajapp_press(AJAPP_MELODY, 2);
    blocks(4);
    CHECK(now().running && now().slot[0] == 0 && now().slot[1] == 2, "pair did not start");
    CHECK(notes_in(T_DRUMS) > 8 && notes_in(T_PLUCK) > 4, "loops are not in their tracks");

    run_to_step(22);
    unsigned before = now().step;
    ajapp_press(AJAPP_DRUMS, 1);                        /* swap in the middle of bar 2 */
    blocks(2);
    CHECK(now().slot[0] == 1 && now().step >= before && now().step <= before + 1, "swap moved the beat: step %u then %u",
          before, now().step);

    run_to_step(33);
    ajapp_press(AJAPP_MELODY, 2);                       /* the playing key again */
    CHECK(now().stopping[1] && now().slot[1] == 2, "melody should wait for the beat");
    run_to_step(35);
    CHECK(now().slot[1] == 2, "melody stopped before the beat");
    run_to_step(37);
    CHECK(now().slot[1] == -1 && notes_in(T_PLUCK) == 0, "melody did not stop on the beat");
    CHECK(now().running && now().slot[0] == 1, "drums should carry on alone");
    ajapp_press(AJAPP_DRUMS, 1);
    run_to_step(41);
    blocks(8);
    CHECK(!now().running && now().slot[0] == -1, "the clock should stop when both sides are silent");

    ajapp_press(AJAPP_MELODY, 5);
    blocks(4);
    CHECK(now().running && now().step < 2, "a new press should restart from the top, step %u", now().step);
}

static void test_pages_dice_playstop(void)
{
    printf("pages, Dice and PLAY/STOP\n");
    fresh();
    ajapp_page_step(AJAPP_DRUMS, 1);
    ajapp_page_step(AJAPP_MELODY, -1);
    CHECK(now().view_page[0] == 1 && now().view_page[1] == 5, "pages %u %u", now().view_page[0], now().view_page[1]);
    ajapp_press(AJAPP_DRUMS, 3);
    ajapp_press(AJAPP_MELODY, 7);
    blocks(4);
    CHECK(now().slot[0] == 11 && now().slot[1] == 47, "keys should play from the shown page: %d %d", now().slot[0], now().slot[1]);
    for (unsigned i = 0; i < 20; ++i) {
        int was = now().slot[i % 2];
        ajapp_dice(i % 2);
        CHECK(now().slot[i % 2] != was && now().slot[i % 2] >= 0 && now().slot[i % 2] < 48, "Dice gave %d after %d",
              now().slot[i % 2], was);
        CHECK(now().view_page[i % 2] == now().slot[i % 2] / 8, "Dice should show its loop's page");
        blocks(10);
    }
    int drum = now().slot[0], melody = now().slot[1];
    ajapp_play_stop();
    blocks(8);
    CHECK(!now().running && now().slot[0] == -1 && now().slot[1] == -1, "PLAY/STOP should stop both");
    ajapp_play_stop();
    blocks(8);
    CHECK(now().running && now().slot[0] == drum && now().slot[1] == melody, "PLAY/STOP should bring the pair back");
}

static void test_holds_finish(void)
{
    printf("held keys and Finish\n");
    fresh();
    ajapp_pick(AJAPP_DRUMS, 0);
    ajapp_pick(AJAPP_MELODY, 2);
    blocks(4);
    struct groove_step loop[GROOVE_STEPS], held[GROOVE_STEPS], back[GROOVE_STEPS];
    groove_sequence_read(T_DRUMS, 0, loop, GROOVE_STEPS);
    ajapp_hold(AJAPP_HOLD_FILL, 1);
    groove_sequence_read(T_DRUMS, 0, held, GROOVE_STEPS);
    ajapp_hold(AJAPP_HOLD_FILL, 0);
    groove_sequence_read(T_DRUMS, 0, back, GROOVE_STEPS);
    CHECK(now().slot[0] == 0 && memcmp(loop, held, sizeof loop) && !memcmp(loop, back, sizeof loop),
          "Fill should replace the drum loop while held and give it back");

    unsigned normal = notes_in(T_PLUCK);
    ajapp_hold(AJAPP_HOLD_FAST, 1);
    CHECK(now().fast && notes_in(T_PLUCK) != 0, "Fast left the melody empty");
    ajapp_hold(AJAPP_HOLD_FAST, 0);
    CHECK(!now().fast && notes_in(T_PLUCK) == normal, "Fast should give the melody back");

    struct groove_view v;
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 1);
    groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.muted && now().muted[0], "drums should be muted while held");
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 0);
    groove_get_view(T_DRUMS, 0, &v);
    CHECK(!v.muted, "drums should come back");
    ajapp_hold(AJAPP_HOLD_MUTE_MELODY, 1);
    groove_get_view(T_PLUCK, 0, &v);
    CHECK(v.muted, "melody should be muted while held");
    ajapp_hold(AJAPP_HOLD_MUTE_MELODY, 0);

    run_to_step(20);
    ajapp_finish();
    CHECK(now().finishing == 1, "Finish should wait for the bar");
    run_to_step(32);
    blocks(2);
    groove_sequence_read(T_DRUMS, 0, held, GROOVE_STEPS);
    CHECK(now().finishing == 2 && (held[32].drums & (1U << 8)), "Finish should crash on the bar line");
    unsigned b = 0;
    for (; b < 4 * 16 * STEP_BLOCKS && now().running; ++b) block();
    CHECK(!now().running && b > 28 * STEP_BLOCKS && b < 36 * STEP_BLOCKS, "Finish should ring about 2 bars, rang %u steps",
          b / STEP_BLOCKS);
    CHECK(now().slot[0] == -1 && now().slot[1] == -1 && notes_in(T_DRUMS) == 0, "Finish should leave both sides silent");
}

static void test_sounds(void)
{
    printf("kits, instruments, octave, Mood and tempo\n");
    const ajb_library_t *lib = &ajb_factory_library;
    fresh();
    ajapp_pick(AJAPP_DRUMS, 0);
    ajapp_pick(AJAPP_MELODY, 3);                        /* Bell Tower, an FM loop */
    blocks(4);
    CHECK(notes_in(T_FM) > 0, "an FM loop should be on the FM track");
    for (unsigned kit = 0; kit < lib->kit_count; ++kit) {
        ajapp_set_kit(kit);
        unsigned wrong = 0;
        for (unsigned i = 0; i < lib->kits[kit]->count; ++i) {
            const ajb_kit_edit_t *e = &lib->kits[kit]->edits[i];
            uint8_t values[GROOVE_TONE_PARAMS] = {0};
            groove_tone_copy(T_DRUMS, e->part, values);
            const fm1_param_t *info = groove_tone_parameter(T_DRUMS, e->part, e->parameter);
            unsigned want = e->part == AJB_PARTS && e->parameter == 0 ? 110 : e->value;
            if (info && want > info->max) want = info->max;
            int later = 0;                          /* a kit may set one thing twice: the last one counts */
            for (unsigned k = i + 1; k < lib->kits[kit]->count; ++k)
                later |= lib->kits[kit]->edits[k].part == e->part && lib->kits[kit]->edits[k].parameter == e->parameter;
            if (!later && (!info || values[e->parameter] != want)) {
                ++wrong;
                printf("    part %u setting %u: wanted %u, has %u (max %u)\n", e->part, e->parameter, want,
                       values[e->parameter], info ? info->max : 0);
            }
        }
        CHECK(!wrong, "kit %s: %u settings did not take", lib->kits[kit]->name, wrong);
    }
    static const uint8_t home[5] = {0, 0, T_PLUCK, T_PSG, T_BASS};      /* by instrument kind */
    for (unsigned i = 0; i < lib->instrument_count; ++i) {
        ajapp_set_instrument(i);
        unsigned kind = lib->instruments[i]->kind, track = kind == AJB_INSTRUMENT_FM ? T_FM : home[kind], elsewhere = 0;
        for (unsigned t = T_FM; t <= T_BASS; ++t) elsewhere += t != track ? notes_in(t) : 0;
        CHECK(notes_in(track) > 0 && !elsewhere, "%s: melody should move to its engine's track", lib->instruments[i]->name);
    }
    ajapp_pick(AJAPP_MELODY, 4);                        /* Glass Rain, another FM loop */
    blocks(2);
    CHECK(now().instrument == 255 && notes_in(T_FM) > 0, "a new melody key should go back to its own instrument");

    struct groove_step low[GROOVE_STEPS], high[GROOVE_STEPS];
    groove_sequence_read(T_FM, 0, low, GROOVE_STEPS);
    ajapp_set_octave(1);
    groove_sequence_read(T_FM, 0, high, GROOVE_STEPS);
    unsigned shifted = 0, sounding = 0;
    for (unsigned s = 0; s < GROOVE_STEPS; ++s) if (low[s].velocity) { ++sounding; shifted += high[s].note == low[s].note + 12; }
    CHECK(sounding && shifted == sounding, "OCT+ should lift every note an octave");
    ajapp_set_octave(0);

    /* Mood: Spooky is C Eb F G Bb. It lands on the next bar, not before. */
    run_to_step(18);
    ajapp_set_mood(2);
    struct groove_step cells[GROOVE_STEPS];
    groove_sequence_read(T_FM, 0, cells, GROOVE_STEPS);
    unsigned kept = 0;
    for (unsigned s = 18; s < 32; ++s) kept += cells[s].note == low[s].note && cells[s].velocity == low[s].velocity;
    CHECK(kept == 14, "Mood changed %u steps of the bar that is playing", 14 - kept);
    run_to_step(34);
    groove_sequence_read(T_FM, 0, cells, GROOVE_STEPS);
    unsigned outside = 0;
    for (unsigned s = 0; s < GROOVE_STEPS; ++s) {
        unsigned pc = cells[s].note % 12;
        outside += cells[s].velocity && pc != 0 && pc != 3 && pc != 5 && pc != 7 && pc != 10;
    }
    CHECK(!outside, "%u notes are outside the Spooky scale after the bar", outside);

    ajapp_pick(AJAPP_MELODY, 3 * 8);                    /* Saria's Song: fixed notes */
    groove_sequence_read(T_FM, 0, low, GROOVE_STEPS);
    ajapp_set_mood(0);
    run_to_step(2);
    groove_sequence_read(T_FM, 0, high, GROOVE_STEPS);
    CHECK(!memcmp(low, high, sizeof low), "a fixed tune should ignore Mood");

    struct groove_view v;
    ajapp_set_bpm(140); groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.tempo == 140 && now().bpm == 140, "tempo %u", v.tempo);
    ajapp_set_bpm(500); groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.tempo == AJAPP_BPM_MAX, "tempo should stop at %u, got %u", AJAPP_BPM_MAX, v.tempo);
    ajapp_set_bpm(5); groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.tempo == AJAPP_BPM_MIN, "tempo should stop at %u, got %u", AJAPP_BPM_MIN, v.tempo);
}

static double energy(unsigned n)
{
    double sum = 0;
    for (unsigned b = 0; b < n; ++b) {
        ajapp_tick();
        const float *out = sim_render();
        int32_t q[FM1_BLOCK_WORDS];
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) q[i] = (int32_t)lrintf(out[i] * 8388608.0f);
        ajapp_audio(q, FM1_BLOCK_FRAMES);
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) { double x = q[i] / 8388608.0; sum += x * x; if (fabs(x) > peak) peak = fabs(x); }
    }
    return sum;
}
static void test_effects(void)
{
    printf("effects and knobs\n");
    fresh();
    ajapp_pick(AJAPP_DRUMS, 0);
    ajapp_pick(AJAPP_MELODY, 2);
    blocks(16 * STEP_BLOCKS);
    uint8_t sends[2];
    groove_sends_get(T_DRUMS, sends);
    CHECK(!sends[0] && !sends[1], "sends should start at zero");
    ajapp_effect(AJAPP_FX_ECHO, 1);
    groove_sends_get(T_PLUCK, sends);
    CHECK(sends[1] > 0 && !sends[0], "Echo should open the delay send");
    ajapp_effect(AJAPP_FX_SPARKLE, 1);
    groove_sends_get(T_DRUMS, sends);
    CHECK(sends[0] > 0, "Sparkle should open the reverb send");
    CHECK(now().effects == 3, "effect bits %u", now().effects);
    ajapp_home();
    groove_sends_get(T_DRUMS, sends);
    CHECK(!sends[0] && !sends[1] && !now().effects, "HOME should switch every effect off");

    run_to_step(0);
    double dry = energy(16 * STEP_BLOCKS);
    static const uint8_t own[3] = {AJAPP_FX_WOBBLE, AJAPP_FX_WAH, AJAPP_FX_CRUNCH};
    for (unsigned i = 0; i < 3; ++i) {
        run_to_step(0);
        ajapp_effect(own[i], 1);
        double wet = energy(16 * STEP_BLOCKS);
        ajapp_effect(own[i], 0);
        blocks(8 * STEP_BLOCKS);
        CHECK(fabs(wet - dry) > 0.02 * dry, "effect %u made no difference (%.3f against %.3f)", own[i], wet, dry);
        CHECK(wet < 4.0 * dry, "effect %u is more than 6 dB louder than dry", own[i]);
    }
    run_to_step(0);
    ajapp_knob(AJAPP_KNOB_SWOOSH, 0);
    double dark = energy(16 * STEP_BLOCKS);
    ajapp_knob(AJAPP_KNOB_SWOOSH, 64);
    CHECK(dark < 0.8 * dry, "Swoosh fully down should take the top off (%.3f against %.3f)", dark, dry);

    struct groove_view v;
    groove_get_view(T_DRUMS, 0, &v);
    unsigned even = v.volume;
    ajapp_knob(AJAPP_KNOB_BALANCE, 127);
    groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.volume < even, "balance to the melody should turn the drums down");
    ajapp_knob(AJAPP_KNOB_BALANCE, 64);
    groove_get_view(T_DRUMS, 0, &v);
    CHECK(v.volume == even, "balance back to the middle should restore the drums");
}

static void test_library(void)
{
    printf("every same-key pair on every page plays, in time and without clipping\n");
    static const unsigned happy = 0;
    double low = 0, high = -999;
    for (unsigned i = 0; i < 48; ++i) {
        double db = render_pair((int)i, (int)i, NULL, 4, &happy, 1, -1);
        CHECK(db > -30.0 && db < -0.5, "page %u key %u peaks at %.2f dBFS", i / 8 + 1, i % 8 + 1, db);
        low = db < low ? db : low; high = db > high ? db : high;
    }
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < 48; ++i) {
            double db = render_pair(side ? -1 : (int)i, side ? (int)i : -1, NULL, 4, &happy, 1, -1);
            double target = side ? -15.0 : -12.0;
            CHECK(db > target - 4.5 && db < target + 1.0, "%s %u.%u alone peaks at %.2f dBFS, wanted about %.0f",
                  side ? "melody" : "drums", i / 8 + 1, i % 8 + 1, db, target);
        }
    printf("  pairs peak between %.2f and %.2f dBFS\n", low, high);
}

int main(void)
{
    test_keys();
    test_pages_dice_playstop();
    test_holds_finish();
    test_sounds();
    test_effects();
    test_library();
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
