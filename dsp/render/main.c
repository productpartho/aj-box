/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Windows command-line renderer: plays loops through the AJ Box engine and
 * writes 44.1 kHz 16-bit stereo WAV files (PRD "How it fits together",
 * workflow step 4: render loops to WAV on the PC and tune by ear).
 *
 *   ajbox_render pairs <dir> [bars] [bpm]        the 8 same-key pairs of every page, in <dir>/page1..6
 *   ajbox_render page <n> <dir> [bars] [bpm]     all 64 drum + melody pairs of page n
 *   ajbox_render one <drum> <melody> <out.wav> [bars] [bpm] [mood]
 *                                                loops as page.key (3.5) or key on page 1; 0 = none
 *   ajbox_render moods <dir>                     every same-key pair touring the 7 moods
 *   ajbox_render demos <dir>                     the controls: effects, holds, kits, instruments, Dice ...
 *   ajbox_render extras <dir>                    sampler demos (placeholders)
 *   ajbox_render levels                          each loop's level alone, for tuning gains
 *   ajbox_render report                          audio and pattern sizes */
#include "ajbox.h"
#include "wav.h"
#include "sea5.h"
#include <ctype.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PRD "Storage math": samples live in the stock SEA5 sample bank, 456 KiB. */
enum { AUDIO_BUDGET = 466944 };

typedef struct { double peak_dbfs; unsigned min_gain_q30, clipped; double seconds; } stats_t;

static void slug(char *out, size_t size, const char *name)
{
    size_t n = 0;
    for (; *name && n + 1 < size; ++name)
        out[n++] = isalnum((unsigned char)*name) ? (char)tolower((unsigned char)*name) : '-';
    out[n] = 0;
}

static double dbfs(int32_t peak) { return peak > 0 ? 20.0 * __builtin_log10((double)peak / 8388608.0) : -999.0; }

/* Press loop key `index` (page * 8 + slot) the way she would: show its page, tap the key. */
static void tap(ajb_engine_t *e, unsigned side, int index)
{
    if (index < 0) return;
    ajb_control(e, (ajb_control_t){AJB_CTL_PAGE, (uint8_t)side, (uint8_t)(index / AJB_SLOTS)});
    ajb_press(e, side, (unsigned)index % AJB_SLOTS);
}

/* Play `bars` bars in each of `count` moods, tap the same keys again (stop
 * on the next beat, 10 ms fade) and keep rendering until the clock stops
 * plus half a second. Each next mood is chosen a beat before its first bar,
 * the way she would turn the knob, and lands on the bar line. */
static int render_moods(const ajb_library_t *lib, int drum, int melody, const char *path, unsigned bars,
                        unsigned bpm, const unsigned *moods, unsigned count, stats_t *st)
{
    ajb_engine_t *e = ajb_engine_create(lib);
    if (!e) return -1;
    ajb_set_bpm(e, bpm);
    ajb_set_mood(e, moods[0]);
    ajb_wav_writer_t w;
    if (ajb_wav_open(&w, path, 2, AJB_RATE)) { ajb_engine_destroy(e); return -1; }
    tap(e, AJB_DRUMS, drum);
    tap(e, AJB_MELODY, melody);
    uint64_t segment = (uint64_t)bars * 16U;   /* steps per mood */
    uint64_t play = segment * count * 661500U / ajb_bpm(e);
    int32_t block[AJB_BLOCK * 2], peak = 0;
    unsigned min_gain = ajb_limiter_gain_q30(), clipped = 0, stopped = 0, tail = 0, next = 1;
    while (tail < AJB_RATE / 2) {
        if (next < count && ajb_now(e) >= (segment * next - 4U) * 661500U / ajb_bpm(e))
            ajb_set_mood(e, moods[next++]);
        if (!stopped && ajb_now(e) >= play) {
            tap(e, AJB_DRUMS, drum);
            tap(e, AJB_MELODY, melody);
            stopped = 1;
        }
        ajb_render(e, block);
        for (unsigned i = 0; i < AJB_BLOCK * 2; ++i) {
            int32_t a = block[i] < 0 ? -block[i] : block[i];
            if (a > peak) peak = a;
        }
        if (ajb_limiter_gain_q30() < min_gain) min_gain = ajb_limiter_gain_q30();
        clipped += ajb_wav_write_q23(&w, block, AJB_BLOCK);
        if (stopped && !ajb_clock_running(e)) tail += AJB_BLOCK;
    }
    st->peak_dbfs = dbfs(peak);
    st->min_gain_q30 = min_gain;
    st->clipped = clipped;
    st->seconds = (double)w.frames / AJB_RATE;
    ajb_wav_close(&w);
    ajb_engine_destroy(e);
    return 0;
}
static int render(const ajb_library_t *lib, int drum, int melody, const char *path,
                  unsigned bars, unsigned bpm, stats_t *st)
{
    static const unsigned happy = AJB_MOOD_HAPPY;
    return render_moods(lib, drum, melody, path, bars, bpm, &happy, 1, st);
}

static const char *const mood_names[AJB_MOODS] = {"happy", "dreamy", "spooky", "magic", "floaty", "faraway",
                                                  "adventure"};

/* "3.5" is page 3 key 5, "5" is key 5 on page 1, "0" is no loop; -1 if neither. */
static int loop_index(const char *text)
{
    const char *dot = strchr(text, '.');
    int page = dot ? atoi(text) : 1, key = atoi(dot ? dot + 1 : text);
    if (key == 0) return -1;
    if (page < 1 || page > (int)ajb_factory_library.page_count || key < 1 || key > AJB_SLOTS) return -2;
    return (page - 1) * AJB_SLOTS + key - 1;
}

static void print_stats(const char *path, const stats_t *st)
{
    /* The limiter's resting gain is +6 dB (Q30 2.0); below that it is limiting. */
    printf("%-58s %5.1fs  peak %6.2f dBFS  limiter min %+5.1f dB  clipped %u\n", path, st->seconds,
           st->peak_dbfs, 20.0 * __builtin_log10(st->min_gain_q30 / 1073741824.0) - 6.02, st->clipped);
}

/* The pairs of page `p`: all 64 (`every`), or the 8 that share a key. */
static int page_pairs(unsigned p, const char *dir, int every, unsigned bars, unsigned bpm)
{
    const ajb_page_t *page = &ajb_factory_library.pages[p];
    int failed = 0;
    for (unsigned d = 0; d < AJB_SLOTS; ++d) {
        for (unsigned m = 0; m < AJB_SLOTS; ++m) {
            if (!page->drums[d] || !page->melody[m] || (!every && d != m)) continue;
            char a[64], b[64], path[512];
            slug(a, sizeof a, page->drums[d]->name);
            slug(b, sizeof b, page->melody[m]->name);
            snprintf(path, sizeof path, "%s/d%u-%s_m%u-%s.wav", dir, d + 1, a, m + 1, b);
            stats_t st;
            if (render(&ajb_factory_library, (int)(p * AJB_SLOTS + d), (int)(p * AJB_SLOTS + m), path, bars, bpm,
                       &st)) {
                fprintf(stderr, "cannot write %s\n", path);
                failed = 1;
                continue;
            }
            print_stats(path, &st);
            failed |= st.clipped != 0;
        }
    }
    return failed;
}

static int pairs(const char *dir, unsigned bars, unsigned bpm)
{
    int failed = 0;
    for (unsigned p = 0; p < ajb_factory_library.page_count; ++p) {
        char sub[400];
        snprintf(sub, sizeof sub, "%s/page%u", dir, p + 1);
        _mkdir(sub);
        failed |= page_pairs(p, sub, 0, bars, bpm);
    }
    return failed;
}

/* Demos: the placeholder pots loop (recorded at 100 BPM) sliced at three
 * tempos to show it follows tempo without changing pitch, the placeholder
 * music box note played as a pentatonic scale, and a 303 bassline (an
 * engine for later pages). */
extern const ajb_sample_t ajb_sample_pots, ajb_sample_musicbox;
static const ajb_drum_loop_t pots_loop = {"Pots and Pans (placeholder)", 16, &ajb_sample_pots, {0}, 11469};   /* 0.35 */
static const ajb_instrument_t box = {.name = "Music Box (placeholder)", .kind = AJB_INSTRUMENT_SAMPLE,
    .sample = &ajb_sample_musicbox, .gain_q15 = 11469};
/* 303 knobs: 0 Cutoff, 1 Reso, 2 EnvMod, 3 Decay, 4 Accent; a round, bouncy bass for later pages. */
static const ajb_instrument_t bass = {.name = "Bouncy Bass", .kind = AJB_INSTRUMENT_BASS, .gain_q15 = 13000,
    .knobs = {{0, 40}, {1, 80}, {2, 70}, {3, 40}, {4, 90}}, .knob_count = 5};
static const ajb_melody_loop_t bass_line = {.name = "Bouncy Bass", .steps = 16, .instrument = &bass,
    .octave = -1, .velocity = 100, .riff = "0.0.3.05.0.3.4.."};
static const ajb_melody_loop_t box_scale = {.name = "Music Box Scale", .steps = 32, .instrument = &box,
    .octave = 1, .velocity = 110, .riff = "0.1.2.3.4.5.6.7.8.9.8.7.6.5.4.3."};
static const ajb_page_t extras_page = {"Extras", {&pots_loop}, {&box_scale, &bass_line}, 0};
static const ajb_library_t extras_library = {.pages = &extras_page, .page_count = 1};

static int extras(const char *dir)
{
    static const unsigned tempos[] = {80, 110, 160};
    int failed = 0;
    char path[512];
    stats_t st;
    for (unsigned i = 0; i < 3; ++i) {
        snprintf(path, sizeof path, "%s/sliced-pots-placeholder_%ubpm.wav", dir, tempos[i]);
        failed |= render(&extras_library, 0, -1, path, 4, tempos[i], &st);
        print_stats(path, &st);
    }
    snprintf(path, sizeof path, "%s/musicbox-placeholder_scale.wav", dir);
    failed |= render(&extras_library, -1, 0, path, 2, 110, &st);
    print_stats(path, &st);
    snprintf(path, sizeof path, "%s/303-bouncy-bass.wav", dir);
    failed |= render(&extras_library, -1, 1, path, 4, 110, &st);
    print_stats(path, &st);
    return failed;
}

/* Mood demos (PRD "Buttons and knobs", Mood): every melody with the drum
 * loop on the same key, 2 bars of each mood in turn, switching on the bar.
 * `one` renders a pair in a single mood. */
static int mood_demos(const char *dir)
{
    static const unsigned tour[AJB_MOODS] = {AJB_MOOD_HAPPY, AJB_MOOD_DREAMY, AJB_MOOD_SPOOKY, AJB_MOOD_MAGIC,
                                             AJB_MOOD_FLOATY, AJB_MOOD_FARAWAY, AJB_MOOD_ADVENTURE};
    const ajb_library_t *lib = &ajb_factory_library;
    int failed = 0;
    for (unsigned p = 0; p < lib->page_count; ++p)
        for (unsigned k = 0; k < AJB_SLOTS; ++k) {
            if (!lib->pages[p].drums[k] || !lib->pages[p].melody[k]) continue;
            char a[64], b[64], path[512];
            slug(a, sizeof a, lib->pages[p].drums[k]->name);
            slug(b, sizeof b, lib->pages[p].melody[k]->name);
            int index = (int)(p * AJB_SLOTS + k);
            stats_t st;
            snprintf(path, sizeof path, "%s/tour_p%u-k%u_%s_%s.wav", dir, p + 1, k + 1, b, a);
            failed |= render_moods(lib, index, index, path, 2, AJB_BPM_DEFAULT, tour, AJB_MOODS, &st);
            print_stats(path, &st);
            failed |= st.clipped != 0;
        }
    return failed;
}

/* ---- control demos (PRD "Control map") ---- */

/* A cue is a control at a time, counted in quarters of a 16th step at 110
 * BPM (64 to the bar), so a press can land just before the bar it joins. */
enum { CUE_DICE = AJB_CTLS, CUE_CHIME, CUE_REC, CUE_REPLAY, CUE_SAVE, CUE_SEQ };
typedef struct { unsigned at; uint8_t type, a, b; } cue_t;
enum { CUES_MAX = 1024, BAR = 64, STEP = 4, EARLY = 2 };
static cue_t cues[CUES_MAX];
static unsigned cue_count;

static void cue(unsigned at, unsigned type, unsigned a, unsigned b)
{
    if (cue_count < CUES_MAX) cues[cue_count++] = (cue_t){at, (uint8_t)type, (uint8_t)a, (uint8_t)b};
}
static unsigned before(unsigned bar) { return bar * BAR - EARLY; }
static void pick(unsigned at, unsigned drum, unsigned melody)
{
    cue(at, AJB_CTL_PICK, AJB_DRUMS, drum);
    cue(at, AJB_CTL_PICK, AJB_MELODY, melody);
}
static uint64_t cue_sample(unsigned at) { return (uint64_t)at * 661500U / (STEP * AJB_BPM_DEFAULT); }

static void apply(ajb_engine_t *e, const cue_t *c)
{
    switch (c->type) {
    case CUE_DICE: ajb_dice(e, c->a); break;
    case CUE_CHIME: ajb_chime(e); break;
    case CUE_REC: ajb_record(e); break;
    case CUE_REPLAY: ajb_replay(e); break;
    case CUE_SAVE: ajb_save_heart(e); break;
    case CUE_SEQ: ajb_next_heart(e); break;
    default: ajb_control(e, (ajb_control_t){c->type, c->a, c->b});
    }
}

/* Play the queued cues for `bars` bars of clock time into <dir>/<name>.wav,
 * stop whatever still plays, and empty the queue. */
static int play_cues(const char *dir, const char *name, unsigned bars)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    for (unsigned i = 1; i < cue_count; ++i) {   /* by time, same-time cues in the order queued */
        cue_t c = cues[i];
        unsigned k = i;
        for (; k > 0 && cues[k - 1].at > c.at; --k) cues[k] = cues[k - 1];
        cues[k] = c;
    }
    ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
    ajb_wav_writer_t w;
    if (!e) return 1;
    if (ajb_wav_open(&w, path, 2, AJB_RATE)) {
        fprintf(stderr, "cannot write %s\n", path);
        ajb_engine_destroy(e);
        cue_count = 0;
        return 1;
    }
    uint64_t end = cue_sample(bars * BAR);
    int32_t block[AJB_BLOCK * 2], peak = 0;
    unsigned min_gain = ajb_limiter_gain_q30(), clipped = 0, next = 0, stopped = 0, tail = 0;
    while (tail < AJB_RATE / 2 && ajb_now(e) < end + 8U * AJB_RATE) {
        for (; next < cue_count && ajb_now(e) >= cue_sample(cues[next].at); ++next) apply(e, &cues[next]);
        if (!stopped && ajb_now(e) >= end) {
            if (ajb_clock_running(e)) ajb_play_stop(e);
            stopped = 1;
        }
        ajb_render(e, block);
        for (unsigned i = 0; i < AJB_BLOCK * 2; ++i) {
            int32_t a = block[i] < 0 ? -block[i] : block[i];
            if (a > peak) peak = a;
        }
        if (ajb_limiter_gain_q30() < min_gain) min_gain = ajb_limiter_gain_q30();
        clipped += ajb_wav_write_q23(&w, block, AJB_BLOCK);
        if (stopped && !ajb_clock_running(e)) tail += AJB_BLOCK;
    }
    stats_t st = {dbfs(peak), min_gain, clipped, (double)w.frames / AJB_RATE};
    ajb_wav_close(&w);
    ajb_engine_destroy(e);
    cue_count = 0;
    print_stats(path, &st);
    return clipped != 0;
}

static int demos(const char *dir)
{
    const ajb_library_t *lib = &ajb_factory_library;
    int failed = 0;

    /* The hello chime on its own. */
    cue(0, CUE_CHIME, 0, 0);
    failed |= play_cues(dir, "01-chime", 1);

    /* Key 1 of every page in turn, 2 bars each: swaps across pages stay in time. */
    for (unsigned p = 0; p < lib->page_count; ++p) pick(p ? before(2 * p) : 0, p * AJB_SLOTS, p * AJB_SLOTS);
    cue(before(2 * lib->page_count), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "02-pages", 2 * lib->page_count + 2);

    /* Each effect for 2 bars with a clean bar between, Stutter held for a bar, then Finish. */
    pick(0, 0, 0);
    for (unsigned fx = AJB_FX_ECHO; fx <= AJB_FX_CRUNCH; ++fx) {
        cue(before(2 + 3 * fx), AJB_CTL_EFFECT, fx, 1);
        cue(before(4 + 3 * fx), AJB_CTL_EFFECT, fx, 0);
    }
    cue(before(18), AJB_CTL_HOLD, AJB_HOLD_STUTTER, 1);
    cue(before(19), AJB_CTL_HOLD, AJB_HOLD_STUTTER, 0);
    cue(before(20), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "03-effects", 22);

    /* The held keys on a page-2 pair: Fill, Fast, mute drums, mute melody, then Finish. */
    pick(0, 8, 8);
    static const struct { uint8_t hold, down, up; } held[] = {
        {AJB_HOLD_FILL, 2, 3}, {AJB_HOLD_FAST, 4, 6}, {AJB_HOLD_MUTE_DRUMS, 7, 8}, {AJB_HOLD_MUTE_MELODY, 9, 10}};
    for (unsigned i = 0; i < 4; ++i) {
        cue(before(held[i].down), AJB_CTL_HOLD, held[i].hold, 1);
        cue(before(held[i].up), AJB_CTL_HOLD, held[i].hold, 0);
    }
    cue(before(11), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "04-fill-fast-mute", 13);

    /* KNOB 1 Swoosh on a page-3 pair: down to dark, back, up to thin, then HOME. */
    pick(0, 16, 16);
    for (unsigned s = 0; s < 64; ++s) cue(2 * BAR + s * STEP, AJB_CTL_KNOB, AJB_KNOB_SWOOSH, 63 - s);
    for (unsigned s = 0; s < 32; ++s) cue(6 * BAR + s * STEP, AJB_CTL_KNOB, AJB_KNOB_SWOOSH, 2 + 2 * s);
    for (unsigned s = 0; s < 64; ++s) cue(8 * BAR + s * STEP, AJB_CTL_KNOB, AJB_KNOB_SWOOSH, 64 + s);
    cue(before(13), AJB_CTL_HOME, 0, 0);
    cue(before(14), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "05-swoosh", 16);

    /* PRESETS: Bounce on each kit for a bar. */
    cue(0, AJB_CTL_PICK, AJB_DRUMS, 0);
    for (unsigned kit = 0; kit < lib->kit_count; ++kit) cue(kit ? before(kit) : 0, AJB_CTL_KIT, 0, kit);
    cue(before(lib->kit_count), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "06-kits", lib->kit_count + 2);

    /* ALGORITHM: Star Steps on each instrument for a bar. */
    cue(0, AJB_CTL_PICK, AJB_MELODY, 0);
    for (unsigned i = 0; i < lib->instrument_count; ++i) cue(i ? before(i) : 0, AJB_CTL_INSTRUMENT, 0, i);
    cue(before(lib->instrument_count), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "07-instruments", lib->instrument_count + 2);

    /* Dice every 2 bars, drums and melody in turn. */
    pick(0, 0, 0);
    for (unsigned i = 0; i < 8; ++i) cue(before(2 + 2 * i), CUE_DICE, i % 2 ? AJB_MELODY : AJB_DRUMS, 0);
    cue(before(18), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "08-dice", 20);

    /* SELECT tempo (80, 140, 180, back to 110), then OCT+ and OCT-, 2 bars of clock time each. */
    pick(0, 10, 9);
    static const uint8_t tempo[] = {80, 140, 180, AJB_BPM_DEFAULT};
    for (unsigned i = 0; i < 4; ++i) cue((2 + 2 * i) * BAR, AJB_CTL_BPM, 0, tempo[i]);
    cue(before(10), AJB_CTL_OCTAVE, 0, 3);
    cue(before(12), AJB_CTL_OCTAVE, 0, 1);
    cue(before(14), AJB_CTL_OCTAVE, 0, 2);
    cue(before(16), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "09-tempo-octave", 18);

    /* SAVE three hearts from three pages, then SEQ steps through them. */
    pick(0, 0, 0);
    cue(1 * BAR, CUE_SAVE, 0, 0);
    pick(before(2), 16, 16);
    cue(3 * BAR, CUE_SAVE, 0, 0);
    pick(before(4), 32, 32);
    cue(5 * BAR, CUE_SAVE, 0, 0);
    for (unsigned i = 0; i < 4; ++i) cue(before(6 + 2 * i), CUE_SEQ, 0, 0);
    cue(before(14), AJB_CTL_FINISH, 0, 0);
    failed |= play_cues(dir, "10-hearts", 16);

    /* REC a 7-bar jam (Echo, a melody swap, a Fill), stop, then ARP plays it back. */
    cue(0, CUE_REC, 0, 0);
    pick(1, 0, 2);
    cue(before(2), AJB_CTL_EFFECT, AJB_FX_ECHO, 1);
    cue(before(4), AJB_CTL_PICK, AJB_MELODY, 4);
    cue(before(5), AJB_CTL_HOLD, AJB_HOLD_FILL, 1);
    cue(before(6), AJB_CTL_HOLD, AJB_HOLD_FILL, 0);
    cue(before(6), AJB_CTL_EFFECT, AJB_FX_ECHO, 0);
    cue(before(7), AJB_CTL_PLAY_STOP, 0, 0);
    cue(7 * BAR + 8, CUE_REC, 0, 0);
    cue(8 * BAR, CUE_REPLAY, 0, 0);
    failed |= play_cues(dir, "11-jam-replay", 16);
    return failed;
}

/* One loop alone for 4 bars: its peak where it meets the limiter, in dB of
 * output full scale (the limiter's +6 dB resting gain maps Q24 0.5 to 0 dBFS).
 * `kit` < 0 keeps the page's own kit. */
static double solo_peak(unsigned side, unsigned index, int kit)
{
    ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
    if (kit >= 0) ajb_set_kit(e, (unsigned)kit);
    tap(e, side, (int)index);
    int32_t block[AJB_BLOCK * 2];
    for (unsigned b = 0; b < 4 * 16 * 6014 / AJB_BLOCK; ++b) ajb_render(e, block);
    uint32_t peak = ajb_take_side_peak(e, side);
    ajb_engine_destroy(e);
    return peak ? 20.0 * __builtin_log10(peak / 8388608.0) : -999.0;
}

/* Every loop's level with the gain that would put it on target (-6 dBFS
 * drums, -9 dBFS melody; an instrument takes the smallest gain any of its
 * loops asks for), then every kit on four page-1 loops so kits can be
 * matched to each other. */
static int levels(void)
{
    const ajb_library_t *lib = &ajb_factory_library;
    double wanted[64];
    for (unsigned i = 0; i < 64; ++i) wanted[i] = 99.0;
    for (unsigned side = 0; side < AJB_SIDES; ++side)
        for (unsigned p = 0; p < lib->page_count; ++p)
            for (unsigned k = 0; k < AJB_SLOTS; ++k) {
                const ajb_drum_loop_t *d = side == AJB_DRUMS ? lib->pages[p].drums[k] : NULL;
                const ajb_melody_loop_t *m = side == AJB_MELODY ? lib->pages[p].melody[k] : NULL;
                if (!d && !m) continue;
                double peak = solo_peak(side, p * AJB_SLOTS + k, -1), target = d ? -6.0 : -9.0;
                double gain = (d ? d->gain_q15 : m->instrument->gain_q15) / 32768.0;
                double suggested = gain * __builtin_pow(10.0, (target - peak) / 20.0);
                printf("%-6s %u.%u %-18s %-14s peak %+6.2f dBFS  gain %.2f -> %.2f\n", d ? "drums" : "melody",
                       p + 1, k + 1, d ? d->name : m->name, d ? "" : m->instrument->name, peak, gain, suggested);
                for (unsigned i = 0; m && i < lib->instrument_count && i < 64; ++i)
                    if (lib->instruments[i] == m->instrument && suggested < wanted[i]) wanted[i] = suggested;
            }
    printf("\nInstrument gains:\n");
    for (unsigned i = 0; i < lib->instrument_count && i < 64; ++i)
        printf("  %-14s %.2f -> %.2f%s\n", lib->instruments[i]->name, lib->instruments[i]->gain_q15 / 32768.0,
               wanted[i] > 90.0 ? 0.0 : wanted[i], wanted[i] > 90.0 ? "  (no loop uses it)" : "");
    printf("\nKits on Bounce, Stomp, Skippy, Cowbell Party (peak dBFS):\n");
    for (unsigned kit = 0; kit < lib->kit_count; ++kit) {
        printf("  %-9s", lib->kits[kit]->name);
        for (unsigned k = 0; k < 4; ++k) printf(" %+6.2f", solo_peak(AJB_DRUMS, k, (int)kit));
        printf("\n");
    }
    return 0;
}

static int report(void)
{
    const ajb_library_t *lib = &ajb_factory_library;
    unsigned audio = 0, patterns = 0;
    printf("Sampled audio (32 kHz SEA5, PRD \"Storage math\"):\n");
    for (unsigned i = 0; i < lib->sample_count; ++i) {
        const ajb_sample_t *s = lib->samples[i];
        unsigned bytes = ajb_sea5_bytes(s->frames);
        audio += bytes;
        printf("  %-28s %6u frames  %6u bytes  %.2f s\n", s->name, s->frames, bytes, (double)s->frames / s->rate);
    }
    printf("  total %u bytes = %.1f%% of the %u-byte sample bank\n", audio, 100.0 * audio / AUDIO_BUDGET,
           AUDIO_BUDGET);
    for (unsigned p = 0; p < lib->page_count; ++p)
        for (unsigned k = 0; k < AJB_SLOTS; ++k) {
            const ajb_drum_loop_t *d = lib->pages[p].drums[k];
            const ajb_melody_loop_t *m = lib->pages[p].melody[k];
            for (unsigned part = 0; d && part < AJB_PARTS; ++part) patterns += d->parts[part] ? d->steps : 0;
            if (m) patterns += m->riff ? m->steps : 4U * AJB_ARP_BARS;
        }
    printf("Pattern text: %u bytes for %u page(s)\n", patterns, lib->page_count);
    return audio > AUDIO_BUDGET;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "pairs"))
        return pairs(argv[2], argc > 3 ? (unsigned)atoi(argv[3]) : 8, argc > 4 ? (unsigned)atoi(argv[4]) : AJB_BPM_DEFAULT);
    if (argc >= 4 && !strcmp(argv[1], "page")) {
        int page = atoi(argv[2]);
        if (page >= 1 && page <= (int)ajb_factory_library.page_count)
            return page_pairs((unsigned)page - 1, argv[3], 1, argc > 4 ? (unsigned)atoi(argv[4]) : 8,
                              argc > 5 ? (unsigned)atoi(argv[5]) : AJB_BPM_DEFAULT);
    }
    if (argc >= 5 && !strcmp(argv[1], "one")) {
        int drum = loop_index(argv[2]), melody = loop_index(argv[3]);
        unsigned mood = AJB_MOOD_HAPPY;
        while (argc > 7 && mood < AJB_MOODS && strcmp(argv[7], mood_names[mood])) ++mood;
        if (drum > -2 && melody > -2 && mood < AJB_MOODS) {
            stats_t st;
            int r = render_moods(&ajb_factory_library, drum, melody, argv[4], argc > 5 ? (unsigned)atoi(argv[5]) : 8,
                                 argc > 6 ? (unsigned)atoi(argv[6]) : AJB_BPM_DEFAULT, &mood, 1, &st);
            if (!r) print_stats(argv[4], &st);
            return r != 0;
        }
    }
    if (argc >= 3 && !strcmp(argv[1], "extras")) return extras(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "moods")) return mood_demos(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "demos")) return demos(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "report")) return report();
    if (argc >= 2 && !strcmp(argv[1], "levels")) return levels();
    fprintf(stderr, "usage: ajbox_render pairs <dir> [bars] [bpm] | page <n> <dir> [bars] [bpm]\n"
                    "       | one <drum> <melody> <out.wav> [bars] [bpm] [mood]   (loops as page.key, 0 = none)\n"
                    "       | moods <dir> | demos <dir> | extras <dir> | levels | report\n");
    return 2;
}
