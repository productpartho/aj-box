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

/* Switch off and on again: the flash stays, everything else starts fresh. */
static void power_cycle(void)
{
    sim_boot(0);
    ajapp_init(&ajb_factory_library);
    peak = 0; played = 0;
}
static void test_memory(void)
{
    printf("hearts, the jam and memory survive switching off; bad data is ignored\n");
    fresh();
    CHECK(now().hearts == 0 && !now().has_jam, "a fresh box should have no hearts and no jam");
    CHECK(ajapp_next_heart() == 0, "SEQ with no hearts should do nothing");
    ajapp_pick(AJAPP_DRUMS, 3); ajapp_pick(AJAPP_MELODY, 12);
    ajapp_set_kit(7); ajapp_effect(AJAPP_FX_ECHO, 1); ajapp_set_bpm(96);
    blocks(20);
    CHECK(ajapp_save_heart() == 1, "the first heart should be number 1");
    ajapp_pick(AJAPP_DRUMS, 20); ajapp_pick(AJAPP_MELODY, 33);
    ajapp_effect(AJAPP_FX_ECHO, 0); ajapp_set_mood(3); ajapp_set_bpm(130);
    blocks(20);
    CHECK(ajapp_save_heart() == 2 && now().hearts == 2, "the second heart should be number 2");
    CHECK(ajapp_next_heart() == 1, "SEQ should go to heart 1");
    blocks(20);
    CHECK(now().slot[0] == 3 && now().slot[1] == 12 && now().kit == 7 && now().bpm == 96 && (now().effects & 1),
          "heart 1 should bring back its pair, kit, tempo and Echo: %d %d kit %u bpm %u fx %u", now().slot[0], now().slot[1],
          now().kit, now().bpm, now().effects);
    CHECK(ajapp_next_heart() == 2, "SEQ should go on to heart 2");
    blocks(20);
    CHECK(now().slot[0] == 20 && now().slot[1] == 33 && now().bpm == 130 && !(now().effects & 1), "heart 2 came back wrong");
    for (unsigned i = 0; i < 7; ++i) { ajapp_set_bpm(100 + i); CHECK(ajapp_save_heart() == (i < 6 ? i + 3 : 1), "heart %u", i + 3); }
    CHECK(now().hearts == 8, "there should be 8 hearts, the ninth replacing the oldest");

    /* What power-on brings back. */
    ajapp_pick(AJAPP_DRUMS, 9); ajapp_pick(AJAPP_MELODY, 41);
    ajapp_page_step(AJAPP_DRUMS, 2); ajapp_set_bpm(88); ajapp_set_mood(2); ajapp_knob(AJAPP_KNOB_SWOOSH, 30);
    blocks(20);
    CHECK(ajapp_save_memory() == 0, "memory should save");
    power_cycle();
    CHECK(now().hearts == 8, "hearts should survive switching off: %u", now().hearts);
    CHECK(!now().running && now().slot[0] == -1, "after switching on nothing should play by itself");
    CHECK(now().bpm == 88 && now().mood == 2 && now().view_page[0] == 2, "tempo, mood and page should come back: %u %u %u",
          now().bpm, now().mood, now().view_page[0]);
    ajapp_play_stop();
    blocks(20);
    CHECK(now().running && now().slot[0] == 9 && now().slot[1] == 41, "PLAY/STOP should bring back the last pair: %d %d",
          now().slot[0], now().slot[1]);
    CHECK(ajapp_next_heart() >= 1, "a heart saved before switching off should still work");

    /* Rule 10: saved by itself 30 s after the last change. */
    ajapp_set_bpm(77);
    blocks(31 * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES);
    power_cycle();
    CHECK(now().bpm == 77, "a change should be saved by itself after 30 s, tempo %u", now().bpm);

    /* A jam: record, keep, switch off, replay. */
    fresh();
    CHECK(ajapp_record() == 1 && now().recording, "REC should start recording");
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 2);
    blocks(32 * STEP_BLOCKS);
    ajapp_effect(AJAPP_FX_WOBBLE, 1);
    blocks(16 * STEP_BLOCKS);
    ajapp_pick(AJAPP_MELODY, 5);
    blocks(16 * STEP_BLOCKS);
    ajapp_play_stop();
    blocks(8 * STEP_BLOCKS);
    CHECK(ajapp_record() == 0 && now().has_jam && !now().recording, "REC again should stop and keep the jam");
    power_cycle();
    CHECK(now().has_jam, "the jam should survive switching off");
    ajapp_replay();
    CHECK(now().replaying, "ARP should start the replay");
    blocks(40 * STEP_BLOCKS);
    CHECK(now().running && now().slot[0] == 0 && now().slot[1] == 2 && (now().effects & (1U << AJAPP_FX_WOBBLE)),
          "half way through, the replay should be on the first pair with Wobble: %d %d fx %u", now().slot[0], now().slot[1],
          now().effects);
    blocks(20 * STEP_BLOCKS);
    CHECK(now().slot[1] == 5, "the replay should make the melody swap, has %d", now().slot[1]);
    blocks(30 * STEP_BLOCKS);
    CHECK(!now().replaying && !now().running, "the replay should end where the jam ended");

    /* Bad data: damage the newest copy of the hearts, then both. */
    fresh();
    ajapp_pick(AJAPP_DRUMS, 1);
    CHECK(ajapp_save_heart() == 1 && ajapp_save_heart() == 2, "two hearts for the damage test");
    sim_flash()[5 * 4096 + 40] ^= 0x5a;                 /* hearts, second copy (the newer) */
    power_cycle();
    CHECK(now().hearts == 1, "a damaged copy should fall back to the older one: %u hearts", now().hearts);
    sim_flash()[4 * 4096 + 40] ^= 0x5a;
    power_cycle();
    CHECK(now().hearts == 0, "with both copies damaged there should be no hearts, not a crash: %u", now().hearts);
    memset(sim_flash() + 10 * 4096, 0, 64);             /* memory record: garbage */
    power_cycle();
    CHECK(now().bpm == AJAPP_BPM_DEFAULT && !now().running, "garbage memory should give the defaults");

    /* Parent settings. */
    ajapp_parent_t parent = {96, 1, 1, 0}, read;
    CHECK(ajapp_parent_set(&parent) == 0, "parent settings should save");
    power_cycle();
    ajapp_parent_get(&read);
    CHECK(read.default_bpm == 96 && read.swap_on_bar == 1 && now().bpm == 96, "parent tempo should come back: %u", now().bpm);
    parent.default_bpm = 250;
    CHECK(ajapp_parent_set(&parent) != 0, "an out-of-range parent tempo should be refused");

    /* Hello chime. */
    fresh();
    ajapp_chime();
    double ringing = energy(FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES);
    blocks(3 * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES);
    double after = energy(FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES / 2);
    CHECK(ringing > 0.01 && after < ringing / 100.0 && !now().running, "the chime should sound for about a second: %.4f then %.5f",
          ringing, after);
}

/* ---- screens (PRD "Screen and look") ---- */

#include "ajdraw.h"
uint32_t sim_frame_revision(void);
static uint16_t pixel(unsigned x, unsigned y) { return sim_pixels()[y * 240 + x]; }
/* Paint the whole screen as it is now. */
static void look(void) { ajscreen_redraw(); ajscreen_draw(fm1_millis(), 15); }
static unsigned count_colour(uint16_t colour)
{
    unsigned n = 0;
    for (unsigned i = 0; i < 240 * 240; ++i) n += sim_pixels()[i] == colour;
    return n;
}

static void test_screens(void)
{
    printf("screens: start-up, play, effects, knob arcs, hearts, Mood, battery and Finish\n");
    static const uint16_t lit[AJAPP_EFFECTS] = {AJD_PINK, AJD_GOLD, AJD_LILAC, AJD_GRAPE, AJD_MAGENTA, AJD_CREAM};
    CHECK(ajd_sin(0) == 0 && ajd_sin(64) == 16384 && ajd_sin(128) == 0 && ajd_sin(192) == -16384 && ajd_sin(32) > 11500 &&
          ajd_sin(32) < 11650, "sine table");

    /* Start-up: "AJ Box" for 1.5 s, then the play screen with nothing playing. */
    fresh();
    ajscreen_init(fm1_millis());
    wait_ms(300); look();
    CHECK(pixel(2, 2) == AJD_PLUM && count_colour(AJD_PINK) > 2000 && count_colour(AJD_LILAC) > 1500,
          "start-up should show AJ in pink and Box in lilac on plum: %u pink, %u lilac", count_colour(AJD_PINK), count_colour(AJD_LILAC));
    wait_ms(1300); look();
    CHECK(pixel(20, 60) == AJD_DUSK && pixel(220, 60) == AJD_DUSK, "with nothing playing both halves should be dim");
    CHECK(pixel(97, 20) == AJD_GRAPE && pixel(145, 20) == AJD_GRAPE, "no beat dot should be lit while stopped");
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) CHECK(pixel(30 + 36 * k, 203) == AJD_DUSK, "effect bubble %u should be dark", k);

    /* Playing: pink drums left, purple melody right, a cream ring round each, the beat dot moves. */
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 0);
    wait_sixteenth(2); look();
    CHECK(pixel(20, 60) == AJD_PINK && pixel(220, 60) == AJD_GRAPE, "playing halves should be pink and purple");
    CHECK(pixel(9, 116) == AJD_CREAM && pixel(231, 116) == AJD_CREAM, "playing halves should have a cream ring");
    CHECK(pixel(63, 80) == AJD_PLUM && pixel(177, 80) == AJD_PLUM, "each half should have its dark picture circle");
    for (unsigned beat = 0; beat < 4; ++beat) {
        run_to_step(beat * 4 + 1); look();
        for (unsigned dot = 0; dot < 4; ++dot)
            CHECK(pixel(97 + 16 * dot, 20) == (dot == beat ? AJD_GOLD : AJD_GRAPE), "beat %u: dot %u is the wrong colour", beat, dot);
    }
    /* The picture hops on the beat and settles before the next one. */
    run_to_step(8); look();
    unsigned up = pixel(63, 73) == AJD_PLUM;
    run_to_step(11); look();
    CHECK(up && pixel(63, 73) == AJD_PINK && pixel(63, 137) == AJD_PLUM, "the drum picture should hop up on the beat and come down");
    /* Page dots follow the shown page. */
    CHECK(pixel(38, 155) == AJD_CREAM && pixel(48, 155) == AJD_MAGENTA, "page dot 1 should be lit on the drum side");
    ajapp_page_step(AJAPP_DRUMS, 1); look();
    CHECK(pixel(38, 155) == AJD_MAGENTA && pixel(48, 155) == AJD_CREAM, "page dot 2 should be lit after a page step");
    ajapp_page_step(AJAPP_DRUMS, -1);

    /* Effects light their bubbles; muting dims a half; REC draws its frame and dot. */
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) {
        ajapp_effect(k, 1); look();
        for (unsigned j = 0; j < AJAPP_EFFECTS; ++j)
            CHECK(pixel(30 + 36 * j, 203) == (j == k ? lit[j] : AJD_DUSK), "effect %u on: bubble %u is the wrong colour", k, j);
        ajapp_effect(k, 0);
    }
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 1); look();
    CHECK(pixel(20, 60) == AJD_DUSK && pixel(220, 60) == AJD_GRAPE, "a muted half should dim, the other stay bright");
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 0);
    look();
    CHECK(pixel(0, 0) == AJD_PLUM && pixel(239, 239) == AJD_PLUM, "no frame while not recording");
    ajapp_record(); look();
    CHECK(pixel(0, 0) == AJD_MAGENTA && pixel(239, 239) == AJD_MAGENTA && pixel(120, 1) == AJD_MAGENTA && pixel(218, 20) != AJD_PLUM,
          "REC should draw a magenta frame and its dot");
    ajapp_record(); look();
    CHECK(pixel(0, 0) == AJD_PLUM, "the frame should go when REC stops");

    /* A knob draws its arc in the middle for a moment, then it goes. */
    ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_SPEED, 127, fm1_millis()); look();
    CHECK(pixel(120, 84) == AJD_LILAC && pixel(152, 116) == AJD_LILAC && pixel(88, 116) == AJD_LILAC,
          "a full turn of the lilac knob should draw a whole lilac circle");
    ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_SPEED, 32, fm1_millis()); look();
    CHECK(pixel(143, 93) == AJD_LILAC && pixel(88, 116) == AJD_DUSK, "a quarter turn should draw only the first quarter");
    ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_SWOOSH, 64, fm1_millis()); look();
    CHECK(pixel(120, 84) == AJD_GOLD && pixel(152, 116) == AJD_DUSK && pixel(88, 116) == AJD_DUSK, "the filter knob in the middle should show the clean dot");
    ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_SWOOSH, 0, fm1_millis()); look();
    CHECK(pixel(88, 116) == AJD_PINK && pixel(152, 116) == AJD_DUSK, "the filter knob turned down should draw to the left");
    wait_ms(1300); look();
    CHECK(pixel(120, 84) == AJD_PLUM || pixel(120, 84) == AJD_PINK || pixel(120, 84) == AJD_GRAPE, "the arc should go after about a second");
    CHECK(pixel(120, 116) == AJD_PLUM, "the gap between the halves should be back");

    /* Hearts: the number, the row, and an empty heart when nothing was saved. */
    ajapp_save_heart();
    ajscreen_event(AJSCREEN_HEART, ajapp_save_heart(), 0, fm1_millis()); look();
    CHECK(pixel(95, 85) == AJD_PINK && pixel(36, 189) == AJD_PINK && pixel(60, 189) == AJD_GOLD && pixel(84, 189) == AJD_DUSK,
          "heart 2 saved: big pink heart, heart 1 pink, heart 2 gold, heart 3 empty");
    unsigned digit = 0;
    for (unsigned y = 91; y < 116; ++y) for (unsigned x = 111; x < 129; ++x) digit += pixel(x, y) == AJD_PLUM;
    CHECK(digit > 60, "the heart should carry its number: %u dark pixels", digit);
    ajscreen_event(AJSCREEN_HEART, 0, 0, fm1_millis()); look();
    CHECK(pixel(95, 85) == AJD_DUSK && pixel(60, 189) == AJD_PINK && pixel(84, 189) == AJD_DUSK,
          "a heart that was not saved should stay dark and add nothing to the row");
    wait_ms(1600); look();
    CHECK(pixel(20, 60) == AJD_PINK, "the play screen should come back after the heart");

    /* Mood: the chosen bubble has the cream ring. */
    ajapp_set_mood(0); ajscreen_event(AJSCREEN_MOOD, 0, 0, fm1_millis()); look();
    CHECK(pixel(30, 187) == AJD_CREAM && pixel(94, 187) == AJD_PLUM && pixel(120, 85) == AJD_GOLD, "Happy: first bubble ringed, a gold sun");
    ajapp_set_mood(3); ajscreen_event(AJSCREEN_MOOD, 0, 0, fm1_millis()); look();
    CHECK(pixel(30, 187) == AJD_PLUM && pixel(94, 187) == AJD_CREAM, "Magic: the middle bubble ringed");
    ajapp_set_mood(6); ajscreen_event(AJSCREEN_MOOD, 0, 0, fm1_millis()); look();
    CHECK(pixel(158, 187) == AJD_CREAM && pixel(94, 187) == AJD_PLUM, "Adventure: the last bubble ringed");
    ajapp_set_mood(0); ajscreen_event(AJSCREEN_HOME, 0, 0, fm1_millis()); look();
    CHECK(pixel(20, 60) == AJD_PINK, "HOME should bring the play screen back");

    /* Only what changed is repainted, a few strips at a time. */
    uint32_t before = sim_frame_revision();
    CHECK(!ajscreen_draw(fm1_millis(), 15) && sim_frame_revision() == before, "nothing changed, so nothing should be repainted");
    ajapp_effect(AJAPP_FX_WAH, 1);
    CHECK(!ajscreen_draw(fm1_millis(), 15) && sim_frame_revision() - before == 3, "an effect should repaint 3 strips, not %u",
          (unsigned)(sim_frame_revision() - before));
    ajapp_effect(AJAPP_FX_WAH, 0); ajscreen_draw(fm1_millis(), 15);
    ajscreen_redraw();
    before = sim_frame_revision();
    unsigned calls = 1;
    while (ajscreen_draw(fm1_millis(), 4) && calls < 10) ++calls;
    CHECK(calls == 4 && sim_frame_revision() - before == 15, "15 strips at 4 a call should take 4 calls, took %u", calls);

    /* Low battery shows the sleepy face once; Finish shows the star. */
    ajscreen_battery(0, 1, fm1_millis()); look();
    CHECK(pixel(70, 120) == AJD_MAGENTA && pixel(120, 100) == AJD_DUSK, "low battery should show the sleepy battery");
    ajscreen_battery(0, 1, fm1_millis());
    wait_ms(2600); look();
    CHECK(pixel(20, 60) == AJD_PINK && pixel(18, 20) == AJD_PLUM, "the play screen should come back, with an empty battery");
    ajapp_finish();
    for (unsigned n = 0; n < 4000 && now().finishing != 2; ++n) block();
    look();
    CHECK(pixel(120, 95) == AJD_GOLD && count_colour(AJD_CREAM) > 800, "Finish should show the gold star and Ta-da");
    for (unsigned n = 0; n < 4000 && now().running; ++n) block();
    wait_ms(1600); look();
    CHECK(pixel(20, 60) == AJD_DUSK && pixel(120, 95) == AJD_PLUM, "after Finish the play screen should be back and quiet");
}

int main(void)
{
    test_keys();
    test_pages_dice_playstop();
    test_holds_finish();
    test_sounds();
    test_effects();
    test_memory();
    test_library();
    test_screens();
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
