/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Desktop renderer for AJ Box on FM1 Open's stock instrument: FM1 Open's own
 * simulator (firmware/web) plays, AJ Box's app logic (ajapp.c) drives it, and
 * this file plays the part of the FM-1 around them: it loads the one-shot
 * sample bank, presses the keys and writes WAVs.
 *
 *   ajstock pair <drum> <melody> <out.wav> [bars] [mood]   loops as page.key, 0 = none
 *   ajstock page <n> <dir>         the 8 same-key pairs of page n
 *   ajstock all <dir>              every page, in <dir>/page1..6
 *   ajstock tour <dir>             key 1 of every page through the 7 moods, 2 bars each
 *   ajstock demos <dir>            the controls: pages, effects, held keys, kits ...
 *   ajstock oneshots <dir>         the 8 synthesized pad sounds as WAVs
 *   ajstock screens <dir>          every screen as a 240 x 240 PNG, and one sheet of them all
 *   ajstock levels <file>          measure every loop alone and write the track
 *                                  volumes that put drums and melodies on target */
#include "ajapp.h"
#include "ajscreen.h"
#include "fm1.h"
#include "instrument.h"
#include "oneshots.h"
#include "wav.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sim_boot(unsigned clear);
const float *sim_render(void);
uint8_t *sim_flash(void);
int sim_sample_bank(unsigned bytes);
uint8_t *sampledrum_encode(const int16_t *pcm, unsigned frames, unsigned *bytes);

static const char *const mood_names[AJAPP_MOODS] = {"happy", "dreamy", "spooky", "magic", "floaty", "faraway", "adventure"};

/* The 8 one-shots as a stock sample bank (firmware/app/samples.c): a 512-byte
 * header, then each sound as SEA5 chunks. Built once. */
static const uint8_t *oneshot_bank(unsigned *bytes)
{
    static uint8_t bank[GROOVE_SAMPLE_BYTES];
    static unsigned total;
    if (!total) {
        struct { uint32_t offset, frames, bytes; char name[16]; } entry[16];
        uint32_t head[8] = {0x53314d46U, 1, 0, 0, 0, 32000, 0, 0};
        static int16_t pcm[AJB_ONESHOT_FRAMES + 320];
        unsigned at = 512;
        memset(entry, 0, sizeof entry);
        for (unsigned pad = 0; pad < 16; ++pad) {
            entry[pad].offset = at;
            if (pad >= AJB_ONESHOTS) continue;
            memset(pcm, 0, sizeof pcm);
            unsigned frames = ajb_oneshot(pad, pcm), size = 0;
            uint8_t *data = sampledrum_encode(pcm, frames, &size);
            if (!data || at + size > sizeof bank) { free(data); continue; }
            memcpy(bank + at, data, size);
            free(data);
            entry[pad].frames = frames;
            entry[pad].bytes = size;
            strncpy(entry[pad].name, ajb_oneshot_names[pad], 15);
            at += size;
            for (unsigned later = pad + 1; later < 16; ++later) entry[later].offset = at;
        }
        head[2] = at;
        memcpy(bank, head, sizeof head);
        memcpy(bank + 32, entry, sizeof entry);
        uint32_t crc = ~groove_crc32(~0U, bank, at);      /* the crc and commit words are still zero here */
        memcpy(bank + 12, &crc, 4);
        total = at;
    }
    *bytes = total;
    return bank;
}

/* Switch on: a fresh FM-1 with the one-shot bank in its flash, AJ Box started. */
static void boot(void)
{
    sim_boot(1);
    unsigned bank_bytes;
    const uint8_t *bank = oneshot_bank(&bank_bytes);
    memcpy(sim_flash() + GROOVE_SAMPLE_OFFSET, bank, bank_bytes);
    if (sim_sample_bank(bank_bytes)) fprintf(stderr, "the one-shot bank did not load\n");
    ajapp_init(&ajb_factory_library);
}

/* ---- playing time ---- */

static ajb_wav_writer_t wav;
static int wav_open;
static double peak;
static const double BAR_BLOCKS = 4.0 * 60.0 / AJAPP_BPM_DEFAULT * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES;

/* One audio block: the control side ticks, the instrument renders, AJ Box's
 * mix effects run, as on the device. */
static void block(void)
{
    ajapp_tick();
    const float *out = sim_render();
    int32_t q[FM1_BLOCK_WORDS];
    int16_t pcm[FM1_BLOCK_WORDS];
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) q[i] = (int32_t)lrintf(out[i] * 8388608.0f);
    ajapp_audio(q, FM1_BLOCK_FRAMES);
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
        double a = fabs(q[i] / 8388608.0);
        if (a > peak) peak = a;
        int32_t v = (q[i] + 128) >> 8;
        pcm[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    if (wav_open) ajb_wav_write(&wav, pcm, FM1_BLOCK_FRAMES);
}
static void blocks(unsigned n) { while (n--) block(); }
/* Play up to bar line `bar` (counted from the start of the file at 110 BPM), less `early` blocks. */
static unsigned played;
static void until(double bar, unsigned early)
{
    unsigned target = (unsigned)(bar * BAR_BLOCKS);
    target = target > early ? target - early : 0;
    while (played < target) { block(); ++played; }
}
static int start(const char *path)
{
    boot();
    peak = 0; played = 0;
    wav_open = path && !ajb_wav_open(&wav, path, 2, FM1_SAMPLE_RATE);
    return path && !wav_open;
}
static double finish(const char *path)
{
    ajapp_status_t st;
    ajapp_status(&st);
    if (st.running && !st.finishing) ajapp_play_stop();
    for (unsigned b = 0; b < 3 * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES; ++b) {      /* let it ring, at most 3 s more */
        block();
        ajapp_status(&st);
        if (!st.running && b > FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES) break;
    }
    double seconds = wav_open ? (double)wav.frames / FM1_SAMPLE_RATE : 0;
    if (wav_open) ajb_wav_close(&wav);
    wav_open = 0;
    double db = peak > 0 ? 20.0 * log10(peak) : -999.0;
    if (path) printf("%-72s %5.1fs  peak %6.2f dBFS\n", path, seconds, db);
    return db;
}
enum { EARLY = 6 };   /* a press about 17 ms before the bar it should land on */

static double render_pair(int drum, int melody, const char *path, unsigned bars, const unsigned *moods, unsigned count,
                          int volume)
{
    if (start(path)) return 0;
    ajapp_debug_volume(volume);
    ajapp_set_mood(moods[0]);
    if (drum >= 0) ajapp_pick(AJAPP_DRUMS, (unsigned)drum);
    if (melody >= 0) ajapp_pick(AJAPP_MELODY, (unsigned)melody);
    for (unsigned m = 1; m < count; ++m) { until(bars * m, 0); ajapp_set_mood(moods[m]); }   /* lands on the next bar */
    until(bars * count, 0);
    return finish(path);
}

static int loop_index(const char *text)
{
    const char *dot = strchr(text, '.');
    int page = dot ? atoi(text) : 1, key = atoi(dot ? dot + 1 : text);
    if (key == 0) return -1;
    if (page < 1 || page > (int)ajb_factory_library.page_count || key < 1 || key > AJB_SLOTS) return -2;
    return (page - 1) * AJB_SLOTS + key - 1;
}
static void slug(char *out, size_t size, const char *name)
{
    size_t n = 0;
    for (; *name && n + 1 < size; ++name) out[n++] = isalnum((unsigned char)*name) ? (char)tolower((unsigned char)*name) : '-';
    out[n] = 0;
}
static void render_page(unsigned p, const char *dir)
{
    static const unsigned happy = 0;
    const ajb_page_t *page = &ajb_factory_library.pages[p];
    for (unsigned k = 0; k < AJB_SLOTS; ++k) {
        char a[64], b[64], path[512];
        slug(a, sizeof a, page->drums[k]->name);
        slug(b, sizeof b, page->melody[k]->name);
        snprintf(path, sizeof path, "%s/k%u_%s_%s.wav", dir, k + 1, a, b);
        render_pair((int)(p * AJB_SLOTS + k), (int)(p * AJB_SLOTS + k), path, 8, &happy, 1, -1);
    }
}

/* ---- the controls, one file each (PRD "Control map") ---- */

static void demos(const char *dir)
{
    const ajb_library_t *lib = &ajb_factory_library;
    char path[512];
#define FILE_NAME(name) (snprintf(path, sizeof path, "%s/%s.wav", dir, name), path)

    /* Key 1 of every page in turn, 2 bars each: swaps across pages stay in time. */
    start(FILE_NAME("01-pages"));
    for (unsigned p = 0; p < lib->page_count; ++p) {
        until(2.0 * p, p ? EARLY : 0);
        ajapp_pick(AJAPP_DRUMS, p * 8); ajapp_pick(AJAPP_MELODY, p * 8);
    }
    until(2.0 * lib->page_count, EARLY); ajapp_finish(); until(2.0 * lib->page_count + 3, 0);
    finish(path);

    /* A swap in the middle of a bar: the new loop carries on from that step. */
    start(FILE_NAME("02-swap-mid-bar"));
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 2);
    until(2.5, 0); ajapp_pick(AJAPP_DRUMS, 1);
    until(4.25, 0); ajapp_pick(AJAPP_MELODY, 3);
    until(6.75, 0); ajapp_pick(AJAPP_DRUMS, 3);
    until(8, 0); ajapp_pick(AJAPP_MELODY, 3);          /* the same key again: stops on the next beat */
    until(10, EARLY); ajapp_finish(); until(13, 0);
    finish(path);

    /* Each effect for 2 bars with a clean bar between, Stutter held for a bar, then Finish. */
    start(FILE_NAME("03-effects"));
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 2);
    for (unsigned fx = AJAPP_FX_ECHO; fx <= AJAPP_FX_CRUNCH; ++fx) {
        until(2 + 3 * fx, EARLY); ajapp_effect(fx, 1);
        until(4 + 3 * fx, EARLY); ajapp_effect(fx, 0);
    }
    until(18, EARLY); ajapp_hold(AJAPP_HOLD_STUTTER, 1);
    until(19, EARLY); ajapp_hold(AJAPP_HOLD_STUTTER, 0);
    until(20, EARLY); ajapp_finish(); until(23, 0);
    finish(path);

    /* The held keys on a page-2 pair: Fill, Fast, mute drums, mute melody, then Finish. */
    start(FILE_NAME("04-fill-fast-mute"));
    ajapp_pick(AJAPP_DRUMS, 8); ajapp_pick(AJAPP_MELODY, 8);
    static const struct { uint8_t hold, down, up; } held[] = {
        {AJAPP_HOLD_FILL, 2, 3}, {AJAPP_HOLD_FAST, 4, 6}, {AJAPP_HOLD_MUTE_DRUMS, 7, 8}, {AJAPP_HOLD_MUTE_MELODY, 9, 10}};
    for (unsigned i = 0; i < 4; ++i) {
        until(held[i].down, EARLY); ajapp_hold(held[i].hold, 1);
        until(held[i].up, EARLY); ajapp_hold(held[i].hold, 0);
    }
    until(11, EARLY); ajapp_finish(); until(14, 0);
    finish(path);

    /* KNOB 1 Swoosh on a page-3 pair: down to dark, back, up to thin, then HOME. */
    start(FILE_NAME("05-swoosh"));
    ajapp_pick(AJAPP_DRUMS, 16); ajapp_pick(AJAPP_MELODY, 16);
    for (unsigned s = 0; s < 64; ++s) { until(2 + s / 16.0, 0); ajapp_knob(AJAPP_KNOB_SWOOSH, 63 - s); }
    for (unsigned s = 0; s < 32; ++s) { until(6 + s / 16.0, 0); ajapp_knob(AJAPP_KNOB_SWOOSH, 2 + 2 * s); }
    for (unsigned s = 0; s < 64; ++s) { until(8 + s / 16.0, 0); ajapp_knob(AJAPP_KNOB_SWOOSH, 64 + s); }
    until(13, EARLY); ajapp_home();
    until(14, EARLY); ajapp_finish(); until(17, 0);
    finish(path);

    /* PRESETS: Bounce on each kit for a bar. */
    start(FILE_NAME("06-kits"));
    ajapp_pick(AJAPP_DRUMS, 0);
    for (unsigned kit = 0; kit < lib->kit_count; ++kit) { until(kit, kit ? EARLY : 0); ajapp_set_kit(kit); }
    until(lib->kit_count, EARLY); ajapp_finish(); until(lib->kit_count + 3, 0);
    finish(path);

    /* ALGORITHM: Harp Hop on each instrument for a bar. */
    start(FILE_NAME("07-instruments"));
    ajapp_pick(AJAPP_MELODY, 2);
    for (unsigned i = 0; i < lib->instrument_count; ++i) { until(i, i ? EARLY : 0); ajapp_set_instrument(i); }
    until(lib->instrument_count, EARLY); ajapp_finish(); until(lib->instrument_count + 3, 0);
    finish(path);

    /* Dice every 2 bars, drums and melody in turn. */
    start(FILE_NAME("08-dice"));
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 0);
    for (unsigned i = 0; i < 8; ++i) { until(2 + 2 * i, EARLY); ajapp_dice(i % 2 ? AJAPP_MELODY : AJAPP_DRUMS); }
    until(18, EARLY); ajapp_finish(); until(21, 0);
    finish(path);

    /* SELECT tempo (80, 140, 180, back to 110), then OCT+ and OCT-; 2 bars of clock time each. */
    start(FILE_NAME("09-tempo-octave"));
    ajapp_pick(AJAPP_DRUMS, 10); ajapp_pick(AJAPP_MELODY, 9);
    static const uint8_t tempo[] = {80, 140, 180, AJAPP_BPM_DEFAULT};
    for (unsigned i = 0; i < 4; ++i) { until(2 + 2 * i, 0); ajapp_set_bpm(tempo[i]); }
    until(10, EARLY); ajapp_set_octave(1);
    until(12, EARLY); ajapp_set_octave(-1);
    until(14, EARLY); ajapp_set_octave(0);
    until(16, EARLY); ajapp_finish(); until(19, 0);
    finish(path);

    /* PLAY/STOP: stop both at once, then bring the same pair back. */
    start(FILE_NAME("10-play-stop"));
    ajapp_pick(AJAPP_DRUMS, 8); ajapp_pick(AJAPP_MELODY, 13);
    until(3, 0); ajapp_play_stop();
    until(4, 0); ajapp_play_stop();
    until(7, EARLY); ajapp_finish(); until(10, 0);
    finish(path);

    /* SAVE three hearts from three pages, then SEQ steps through them. */
    start(FILE_NAME("11-hearts"));
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 0);
    until(1, 0); ajapp_save_heart();
    until(2, EARLY); ajapp_pick(AJAPP_DRUMS, 16); ajapp_pick(AJAPP_MELODY, 16);
    until(3, 0); ajapp_save_heart();
    until(4, EARLY); ajapp_pick(AJAPP_DRUMS, 32); ajapp_pick(AJAPP_MELODY, 32);
    until(5, 0); ajapp_save_heart();
    for (unsigned i = 0; i < 4; ++i) { until(6 + 2 * i, EARLY); ajapp_next_heart(); }
    until(14, EARLY); ajapp_finish(); until(17, 0);
    finish(path);

    /* REC a jam (Echo, a melody swap, a Fill), stop, then ARP plays it back. */
    start(FILE_NAME("12-jam-replay"));
    ajapp_record();
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 2);
    until(2, EARLY); ajapp_effect(AJAPP_FX_ECHO, 1);
    until(4, EARLY); ajapp_pick(AJAPP_MELODY, 4);
    until(5, EARLY); ajapp_hold(AJAPP_HOLD_FILL, 1);
    until(6, EARLY); ajapp_hold(AJAPP_HOLD_FILL, 0); ajapp_effect(AJAPP_FX_ECHO, 0);
    until(7, EARLY); ajapp_play_stop();
    until(7.5, 0); ajapp_record();
    until(8, 0); ajapp_replay();
    until(16, 0);
    finish(path);

    /* The hello chime on its own. */
    start(FILE_NAME("00-chime"));
    ajapp_chime();
    until(1, 0);
    finish(path);
#undef FILE_NAME
}

/* ---- levels ---- */

/* The track volume (1..127) that brings a loop alone closest to `target` dBFS without passing it. */
static unsigned find_volume(unsigned side, int index, double target, double *reached)
{
    static const unsigned happy = 0;
    unsigned low = 1, high = 127;
    while (low < high) {
        unsigned mid = (low + high + 1) / 2;
        if (render_pair(side ? -1 : index, side ? index : -1, NULL, 4, &happy, 1, (int)mid) <= target) low = mid;
        else high = mid - 1;
    }
    *reached = render_pair(side ? -1 : index, side ? index : -1, NULL, 4, &happy, 1, (int)low);
    return low;
}
static int levels(const char *file, double drum_target, double melody_target)
{
    const ajb_library_t *lib = &ajb_factory_library;
    static uint8_t found[2][48];
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < lib->page_count * AJB_SLOTS && i < 48; ++i) {
            const char *name = side ? lib->pages[i / 8].melody[i % 8]->name : lib->pages[i / 8].drums[i % 8]->name;
            double reached, target = side ? melody_target : drum_target;
            found[side][i] = (uint8_t)find_volume(side, (int)i, target, &reached);
            printf("%-6s %u.%u %-18s volume %3u  peak %7.2f dBFS%s\n", side ? "melody" : "drums", i / 8 + 1, i % 8 + 1,
                   name, found[side][i], reached, reached < target - 2.0 ? "  SHORT" : "");
        }
    FILE *f = fopen(file, "w");
    if (!f) return 1;
    fprintf(f, "/* Written by `ajstock levels`: the stock track volume that brings each loop alone\n"
               " * close to %.0f dBFS (drums) or %.0f dBFS (melody) at the simulator output. Row 1\n"
               " * drums, row 2 melodies, 8 per page. Re-run after changing a pattern, kit or sound. */\n",
            drum_target, melody_target);
    for (unsigned side = 0; side < 2; ++side) {
        fprintf(f, "    {");
        for (unsigned i = 0; i < 48; ++i) fprintf(f, "%s%u", i ? (i % 8 ? ", " : ",\n     ") : "", found[side][i]);
        fprintf(f, "},\n");
    }
    fclose(f);
    return 0;
}

/* ---- screens ---- */

const uint16_t *sim_pixels(void);

/* A plain PNG (no compression) of RGB bytes. */
static uint32_t crc32_of(uint32_t crc, const uint8_t *p, size_t n)
{
    crc = ~crc;
    while (n--) { crc ^= *p++; for (int k = 0; k < 8; ++k) crc = crc >> 1 ^ (crc & 1 ? 0xEDB88320U : 0); }
    return ~crc;
}
static void png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t n)
{
    uint8_t head[8] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n,
                       (uint8_t)type[0], (uint8_t)type[1], (uint8_t)type[2], (uint8_t)type[3]};
    uint32_t crc = crc32_of(crc32_of(0, head + 4, 4), data, n);
    uint8_t tail[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8), (uint8_t)crc};
    fwrite(head, 1, 8, f); fwrite(data, 1, n, f); fwrite(tail, 1, 4, f);
}
static int png_write(const char *path, const uint8_t *rgb, unsigned w, unsigned h)
{
    static const uint8_t magic[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    size_t line = 1 + 3 * (size_t)w, raw = h * line, pieces = (raw + 65534) / 65535;
    uint8_t *z = malloc(2 + raw + 5 * pieces + 4), *p = z, *lines = malloc(raw);
    uint8_t ihdr[13] = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w,
                        (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 2, 0, 0, 0};
    for (unsigned y = 0; y < h; ++y) { lines[y * line] = 0; memcpy(lines + y * line + 1, rgb + (size_t)y * 3 * w, 3 * w); }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; ++i) { a = (a + lines[i]) % 65521; b = (b + a) % 65521; }
    *p++ = 0x78; *p++ = 0x01;
    for (size_t at = 0; at < raw; at += 65535) {
        size_t n = raw - at > 65535 ? 65535 : raw - at;
        *p++ = at + n == raw; *p++ = (uint8_t)n; *p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)~n; *p++ = (uint8_t)(~n >> 8);
        memcpy(p, lines + at, n); p += n;
    }
    *p++ = (uint8_t)(b >> 8); *p++ = (uint8_t)b; *p++ = (uint8_t)(a >> 8); *p++ = (uint8_t)a;
    fwrite(magic, 1, 8, f);
    png_chunk(f, "IHDR", ihdr, 13); png_chunk(f, "IDAT", z, (uint32_t)(p - z)); png_chunk(f, "IEND", z, 0);
    free(z); free(lines);
    return fclose(f);
}

enum { SHEET_COLUMNS = 4, SHEET_ROWS = 4, SHEET_GAP = 8, SHEET_W = SHEET_COLUMNS * (240 + SHEET_GAP) + SHEET_GAP,
       SHEET_H = SHEET_ROWS * (240 + SHEET_GAP) + SHEET_GAP };
static uint8_t sheet[SHEET_W * SHEET_H * 3];
static unsigned shots;

static void wait_ms(unsigned ms) { uint32_t end = fm1_millis() + ms; while ((int32_t)(end - fm1_millis()) > 0) block(); }
/* Play on to the next 16th numbered `sixteenth` within its beat (0 = on the beat). */
static void wait_sixteenth(unsigned sixteenth)
{
    ajapp_status_t st;
    for (unsigned n = 0; n < 4000; ++n) { block(); ajapp_status(&st); if (st.running && (st.step & 3) == sixteenth) return; }
}
/* Draw the whole screen as it is now and save it, true size, and into the sheet. */
static void shot(const char *dir, const char *name)
{
    static uint8_t rgb[240 * 240 * 3];
    char path[512];
    ajscreen_redraw();
    ajscreen_draw(fm1_millis(), 15);
    const uint16_t *px = sim_pixels();
    for (unsigned i = 0; i < 240 * 240; ++i) {
        unsigned r = px[i] >> 11, g = px[i] >> 5 & 63, b = px[i] & 31;
        rgb[3 * i] = (uint8_t)(r << 3 | r >> 2); rgb[3 * i + 1] = (uint8_t)(g << 2 | g >> 4); rgb[3 * i + 2] = (uint8_t)(b << 3 | b >> 2);
    }
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    if (!png_write(path, rgb, 240, 240)) printf("%s\n", path);
    if (shots < SHEET_COLUMNS * SHEET_ROWS) {
        unsigned x0 = SHEET_GAP + shots % SHEET_COLUMNS * (240 + SHEET_GAP), y0 = SHEET_GAP + shots / SHEET_COLUMNS * (240 + SHEET_GAP);
        for (unsigned y = 0; y < 240; ++y) memcpy(sheet + ((size_t)(y0 + y) * SHEET_W + x0) * 3, rgb + y * 720, 720);
    }
    ++shots;
}

/* Every screen, in the states the designs show and a few they do not. */
static void screens(const char *dir)
{
    char path[512];
    memset(sheet, 40, sizeof sheet);
    shots = 0;
    boot();
    ajscreen_init(fm1_millis());
    wait_ms(400); shot(dir, "01-hello");
    wait_ms(1300); shot(dir, "02-home-nothing-playing");
    ajapp_pick(AJAPP_DRUMS, 0); ajapp_pick(AJAPP_MELODY, 0);
    wait_sixteenth(2); wait_sixteenth(0); shot(dir, "03-play");
    ajapp_effect(AJAPP_FX_ECHO, 1); ajapp_effect(AJAPP_FX_SPARKLE, 1);
    ajapp_knob(AJAPP_KNOB_AMOUNT, 89); ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_AMOUNT, 89, fm1_millis());
    wait_sixteenth(2); shot(dir, "04-effects-and-knob");
    wait_ms(1300);
    ajapp_effect(AJAPP_FX_ECHO, 0); ajapp_effect(AJAPP_FX_SPARKLE, 0);
    ajapp_record(); wait_sixteenth(1); shot(dir, "05-recording");
    ajapp_record();
    ajapp_save_heart(); ajapp_save_heart();
    ajscreen_event(AJSCREEN_HEART, ajapp_save_heart(), 0, fm1_millis());
    wait_ms(500); shot(dir, "06-heart-3-saved");
    ajscreen_event(AJSCREEN_HEART, 0, 0, fm1_millis());
    wait_ms(100); shot(dir, "07-heart-not-saved");
    ajscreen_event(AJSCREEN_HOME, 0, 0, fm1_millis());
    ajapp_set_mood(0); ajscreen_event(AJSCREEN_MOOD, 0, 0, fm1_millis());
    wait_ms(300); shot(dir, "08-mood-happy");
    ajapp_set_mood(5); ajscreen_event(AJSCREEN_MOOD, 0, 0, fm1_millis());
    wait_ms(300); shot(dir, "09-mood-faraway");
    ajapp_set_mood(0); ajscreen_event(AJSCREEN_HOME, 0, 0, fm1_millis());
    ajapp_pick(AJAPP_DRUMS, 3 * 8 + 6); ajapp_pick(AJAPP_MELODY, 3 * 8 + 6);
    ajapp_effect(AJAPP_FX_WOBBLE, 1); ajapp_effect(AJAPP_FX_WAH, 1); ajapp_effect(AJAPP_FX_CRUNCH, 1); ajapp_effect(AJAPP_FX_STUTTER, 1);
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 1);
    ajapp_set_bpm(170);
    wait_sixteenth(0); wait_sixteenth(3); shot(dir, "10-page4-drums-muted-fast");
    ajapp_hold(AJAPP_HOLD_MUTE_DRUMS, 0);
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) ajapp_effect(k, 0);
    ajapp_set_bpm(70);
    ajapp_set_kit(7); ajscreen_event(AJSCREEN_KIT, 0, 0, fm1_millis());
    wait_sixteenth(1); shot(dir, "11-kit-picture-slow");
    ajapp_knob(AJAPP_KNOB_SWOOSH, 30); ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_SWOOSH, 30, fm1_millis());
    ajapp_dice(AJAPP_MELODY); ajscreen_event(AJSCREEN_DICE, AJAPP_MELODY, 0, fm1_millis());
    wait_ms(150); shot(dir, "12-swoosh-knob-and-dice");
    ajapp_knob(AJAPP_KNOB_SWOOSH, 64);
    ajapp_set_instrument(9); ajscreen_event(AJSCREEN_INSTRUMENT, 0, 0, fm1_millis());
    wait_ms(500); shot(dir, "13-instrument-picture");
    ajapp_knob(AJAPP_KNOB_BALANCE, 100); ajscreen_event(AJSCREEN_KNOB, AJAPP_KNOB_BALANCE, 100, fm1_millis());
    wait_ms(200); shot(dir, "14-balance-knob");
    ajapp_knob(AJAPP_KNOB_BALANCE, 64); ajscreen_event(AJSCREEN_HOME, 0, 0, fm1_millis());
    ajapp_finish();
    for (unsigned n = 0; n < 4000; ++n) { ajapp_status_t st; block(); ajapp_status(&st); if (st.finishing == 2) break; }
    wait_ms(600); shot(dir, "15-finish");
    ajscreen_battery(0, 1, fm1_millis());
    wait_ms(2000); shot(dir, "16-battery-low");
    snprintf(path, sizeof path, "%s/all-screens.png", dir);
    if (!png_write(path, sheet, SHEET_W, SHEET_H)) printf("%s\n", path);
}

int main(int argc, char **argv)
{
    static const unsigned tour[AJAPP_MOODS] = {0, 1, 2, 3, 4, 5, 6};
    if (argc >= 5 && !strcmp(argv[1], "pair")) {
        int drum = loop_index(argv[2]), melody = loop_index(argv[3]);
        unsigned mood = 0;
        while (argc > 6 && mood < AJAPP_MOODS && strcmp(argv[6], mood_names[mood])) ++mood;
        if (drum > -2 && melody > -2 && mood < AJAPP_MOODS) {
            render_pair(drum, melody, argv[4], argc > 5 ? (unsigned)atoi(argv[5]) : 8, &mood, 1, -1);
            return 0;
        }
    }
    if (argc >= 4 && !strcmp(argv[1], "page") && atoi(argv[2]) >= 1 && atoi(argv[2]) <= (int)ajb_factory_library.page_count) {
        render_page((unsigned)atoi(argv[2]) - 1, argv[3]);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "all")) {
        for (unsigned p = 0; p < ajb_factory_library.page_count; ++p) {
            char dir[400];
            snprintf(dir, sizeof dir, "%s/page%u", argv[2], p + 1);
            render_page(p, dir);
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "tour")) {
        for (unsigned p = 0; p < ajb_factory_library.page_count; ++p) {
            char path[512];
            snprintf(path, sizeof path, "%s/moods_page%u-key1.wav", argv[2], p + 1);
            render_pair((int)(p * 8), (int)(p * 8), path, 2, tour, AJAPP_MOODS, -1);
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "demos")) { demos(argv[2]); return 0; }
    if (argc >= 3 && !strcmp(argv[1], "oneshots")) {          /* each pad's sound as its own 32 kHz WAV */
        static int16_t pcm[AJB_ONESHOT_FRAMES + 320];
        for (unsigned pad = 0; pad < AJB_ONESHOTS; ++pad) {
            char path[512];
            snprintf(path, sizeof path, "%s/pad%u-%s.wav", argv[2], pad + 1, ajb_oneshot_names[pad]);
            unsigned frames = ajb_oneshot(pad, pcm);
            ajb_wav_writer_t w;
            if (ajb_wav_open(&w, path, 1, 32000)) return 1;
            ajb_wav_write(&w, pcm, frames);
            ajb_wav_close(&w);
            printf("%s  %.2f s\n", path, frames / 32000.0);
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "screens")) { screens(argv[2]); return 0; }
    if (argc >= 3 && !strcmp(argv[1], "levels"))
        return levels(argv[2], argc > 3 ? atof(argv[3]) : -12.0, argc > 4 ? atof(argv[4]) : -15.0);
    fprintf(stderr, "usage: ajstock pair <drum> <melody> <out.wav> [bars] [mood] | page <n> <dir> | all <dir>\n"
                    "       | tour <dir> | demos <dir> | oneshots <dir> | screens <dir> | levels <volumes.inc> [drum dBFS] [melody dBFS]\n");
    return 2;
}
