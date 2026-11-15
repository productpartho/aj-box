/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box on FM1 Open's stock instrument (docs/s2-design.md), desktop build:
 * turns AJ Box loops into the stock sequencer's cells, sets kits and
 * instruments through the stock calls, and renders WAVs from FM1 Open's own
 * simulator. The device will make the same calls.
 *
 *   ajstock pair <drum> <melody> <out.wav> [bars] [mood]   loops as page.key, 0 = none
 *   ajstock page <n> <dir>         the 8 same-key pairs of page n
 *   ajstock all <dir>              every page, in <dir>/page1..6
 *   ajstock tour <dir>             key 1 of every page through the 7 moods, 2 bars each
 *   ajstock oneshots <dir>         the 8 synthesized pad sounds as WAVs
 *   ajstock levels <file>          measure every loop alone and write the track
 *                                  volumes that put drums and melodies on target */
#include "ajbox_library.h"
#include "instrument.h"
#include "drum808.h"
#include "fm_patch.h"
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

/* Track use (docs/s2-design.md). Tracks 7 and 8 stay off. */
enum { T_DRUMS, T_SAMPLER, T_FM, T_PSG, T_PLUCK, T_BASS, T_USED };
enum { STEPS = GROOVE_STEPS, BPM = 110 };
/* PRD "Buttons and knobs", Mood: seven 5-note scales and their roots. */
enum { MOODS = 7 };
static const char *const mood_names[MOODS] = {"happy", "dreamy", "spooky", "magic", "floaty", "faraway", "adventure"};
static const uint8_t mood_scale[MOODS][5] = {
    {0, 2, 4, 7, 9}, {0, 2, 4, 7, 9}, {0, 3, 5, 7, 10}, {0, 2, 4, 6, 8}, {0, 2, 5, 7, 10}, {0, 2, 3, 7, 8}, {0, 1, 4, 7, 8}};
static const uint8_t mood_root[MOODS] = {60, 65, 60, 60, 60, 60, 60};

/* Track volumes that put each loop on its level, measured by `ajstock levels`. */
static const uint8_t loop_volume[2][48] = {
#include "volumes.inc"
};

static int degree_of(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static int degree_note(const ajb_melody_loop_t *m, int degree, unsigned mood)
{
    if (m->notes && m->note_count)
        return 60 + 12 * m->octave + m->notes[degree % m->note_count] + 12 * (degree / m->note_count);
    return mood_root[mood] + 12 * m->octave + mood_scale[mood][degree % 5] + 12 * (degree / 5);
}

/* One note of `length` steps: the same note in each cell, tied to the next. */
static void put_note(struct groove_step *cells, unsigned count, unsigned at, unsigned length, int note, unsigned velocity)
{
    if (note < 1 || note > 127) return;
    for (unsigned i = 0; i < length && at + i < count; ++i) {
        cells[at + i].note = (uint8_t)note;
        cells[at + i].velocity = (uint8_t)velocity;
        cells[at + i].drums = (uint16_t)((15U << 5) | (i + 1 < length ? 1U << 12 : 0U));   /* full gate, tie */
    }
}

/* A melody loop as `count` stock cells (64, or 48 for a waltz). */
static unsigned melody_cells(const ajb_melody_loop_t *m, unsigned mood, struct groove_step *cells)
{
    unsigned count = m->steps == 48 ? 48 : STEPS;
    /* The chip's 16 volume steps fall about 3 dB for every 8 of velocity, so lift it: 100 and up is full. */
    unsigned velocity = m->instrument->kind == AJB_INSTRUMENT_PSG && m->velocity + 27 > 127 ? 127
                      : m->instrument->kind == AJB_INSTRUMENT_PSG ? m->velocity + 27U : m->velocity;
    memset(cells, 0, STEPS * sizeof *cells);
    for (unsigned s = 0; s < count; ++s) {
        unsigned pos = s % m->steps;
        if (m->riff) {
            int degree = degree_of(m->riff[pos]);
            if (degree < 0) continue;
            unsigned length = 1;
            while (pos + length < m->steps && m->riff[pos + length] == '-') ++length;
            put_note(cells, count, s, length, degree_note(m, degree, mood), velocity);
            continue;
        }
        if (!m->rate || pos % m->rate) continue;
        const char *text = m->chord[(pos / 16U) % AJB_ARP_BARS];
        if (!text) text = m->chord[0];
        int chord[AJB_CHORD_MAX];
        unsigned n_chord = 0;
        for (; text && n_chord < AJB_CHORD_MAX && degree_of(text[n_chord]) >= 0; ++n_chord) chord[n_chord] = degree_of(text[n_chord]);
        unsigned tones = n_chord * (m->octaves ? m->octaves : 1U);
        if (!tones) continue;
        unsigned n = pos / m->rate, k;
        if (m->dir == AJB_ARP_DOWN) k = tones - 1U - n % tones;
        else if (m->dir == AJB_ARP_BOUNCE) {
            unsigned period = tones > 1 ? 2U * tones - 2U : 1U, i = n % period;
            k = i < tones ? i : period - i;
        } else if (m->dir == AJB_ARP_RANDOM) k = hash32(n * 2654435761U + m->steps) % tones;
        else k = n % tones;
        put_note(cells, count, s, m->rate, degree_note(m, chord[k % n_chord] + 5 * (int)(k / n_chord), mood), velocity);
    }
    return count;
}

/* A drum loop as cells. A stock drum step has one velocity for all its
 * drums, so the loudest hit sets it (Partho compared this with three tracks
 * split by loudness on Nov 14, 2026 and chose one track). */
static void drum_cells(const ajb_drum_loop_t *d, struct groove_step cells[STEPS])
{
    static const uint8_t level[3] = {57, 99, 127};   /* 'o' 0.45, 'x' 0.78, 'X' 1.0 of full */
    memset(cells, 0, STEPS * sizeof cells[0]);
    for (unsigned s = 0; s < STEPS; ++s)
        for (unsigned p = 0; p < AJB_PARTS; ++p) {
            if (!d->parts[p]) continue;
            char c = d->parts[p][s % d->steps];
            int rank = c == 'o' ? 0 : c == 'x' ? 1 : c == 'X' ? 2 : -1;
            if (rank < 0) continue;
            struct groove_step *cell = &cells[s];
            cell->drums |= (uint16_t)(1U << p);
            if (level[rank] > cell->velocity) cell->velocity = level[rank];
        }
}

/* Set one sound setting to an absolute value through the stock edit call. */
static void tone_set(unsigned track, unsigned part, unsigned parameter, unsigned value)
{
    uint8_t now[GROOVE_TONE_PARAMS] = {0};
    const fm1_param_t *info = groove_tone_parameter(track, part, parameter);
    if (!info || parameter >= GROOVE_TONE_PARAMS) return;
    groove_tone_copy(track, part, now);
    if (value > info->max) value = info->max;
    if (value != now[parameter]) groove_tone_edit(track, part, parameter, (int)value - (int)now[parameter]);
}
/* The kits were written with the kit level at 40 for AJ Box's old mixer. The
 * stock instrument is quieter, so the kit level is opened up here and the
 * track volume does the balancing. */
static void apply_kit(unsigned track, const ajb_kit_t *kit)
{
    for (unsigned i = 0; kit && i < kit->count; ++i) {
        int kit_level = kit->edits[i].part == AJB_PARTS && kit->edits[i].parameter == 0;
        tone_set(track, kit->edits[i].part, kit->edits[i].parameter, kit_level ? 110U : kit->edits[i].value);
    }
}
static unsigned melody_track(const ajb_instrument_t *in)
{
    return in->kind == AJB_INSTRUMENT_PSG ? T_PSG : in->kind == AJB_INSTRUMENT_PLUCK ? T_PLUCK
         : in->kind == AJB_INSTRUMENT_BASS ? T_BASS : T_FM;
}
static void apply_instrument(const ajb_instrument_t *in)
{
    unsigned track = melody_track(in);
    if (in->kind == AJB_INSTRUMENT_FM) {
        uint8_t patch[156];
        groove_fm_program(patch, in->program);
        groove_fm_import(track, patch);
        return;
    }
    for (unsigned k = 0; k < in->knob_count; ++k) tone_set(track, 0, in->knobs[k].parameter, in->knobs[k].value);
}
static void set_volume(unsigned track, unsigned volume)
{
    struct groove_view v;
    groove_get_view(track, 0, &v);
    groove_control_edit(track, 0, (int)volume - (int)v.volume, 0);
}

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

static void boot(void)
{
    static const uint8_t source[T_USED] = {GROOVE_808, GROOVE_SAMPLE, GROOVE_DX7, GROOVE_PSG, GROOVE_PLUCK, GROOVE_303};
    static const struct groove_step empty[STEPS];
    sim_boot(1);
    unsigned bank_bytes;
    const uint8_t *bank = oneshot_bank(&bank_bytes);
    memcpy(sim_flash() + GROOVE_SAMPLE_OFFSET, bank, bank_bytes);
    if (sim_sample_bank(bank_bytes)) fprintf(stderr, "the one-shot bank did not load\n");
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (t < T_USED && groove_source_type(t) != source[t]) groove_source_set(t, source[t]);
        groove_sequence_replace(t, 0, empty, STEPS);
        if (groove_sequence_length(t) != STEPS) groove_sequence_length_set(t, STEPS);
        struct groove_view state;                   /* the start-up project mutes some tracks */
        groove_get_view(t, 0, &state);
        if (state.muted) groove_mix_toggle(t, 0);
    }
    struct groove_view v;
    groove_get_view(0, 0, &v);
    groove_tempo_edit(BPM - (int)v.tempo);
}

/* Put a drum loop in its track: kit, level, cells. `volume` < 0 uses the table. */
static void set_drums(int index, int volume)
{
    const ajb_library_t *lib = &ajb_factory_library;
    const ajb_page_t *page = &lib->pages[index / AJB_SLOTS];
    const ajb_drum_loop_t *d = page->drums[index % AJB_SLOTS];
    if (!d) return;
    struct groove_step cells[STEPS];
    drum_cells(d, cells);
    apply_kit(T_DRUMS, lib->kits[page->kit]);
    set_volume(T_DRUMS, volume < 0 ? loop_volume[0][index] : (unsigned)volume);
    groove_sequence_replace(T_DRUMS, 0, cells, STEPS);
    /* The one-shot pads: the same loop's other half, on the sampler track. */
    static const uint8_t level[3] = {57, 99, 127};
    memset(cells, 0, sizeof cells);
    for (unsigned s = 0; s < STEPS; ++s)
        for (unsigned p = 0; p < AJB_PADS; ++p) {
            char c = d->pads[p] ? d->pads[p][s % d->steps] : '.';
            int rank = c == 'o' ? 0 : c == 'x' ? 1 : c == 'X' ? 2 : -1;
            if (rank < 0) continue;
            cells[s].drums |= (uint16_t)(1U << p);
            if (level[rank] > cells[s].velocity) cells[s].velocity = level[rank];
        }
    set_volume(T_SAMPLER, volume < 0 ? loop_volume[0][index] : (unsigned)volume);
    groove_sequence_replace(T_SAMPLER, 0, cells, STEPS);
}
/* Put a melody loop in its engine's track. `first` also sets its sound and level. */
static void set_melody(int index, unsigned mood, int volume, int first)
{
    const ajb_melody_loop_t *m = ajb_factory_library.pages[index / AJB_SLOTS].melody[index % AJB_SLOTS];
    struct groove_step cells[STEPS];
    unsigned track = melody_track(m->instrument), count = melody_cells(m, mood, cells);
    if (first) {
        apply_instrument(m->instrument);
        set_volume(track, volume < 0 ? loop_volume[1][index] : (unsigned)volume);
        if (count != STEPS) groove_sequence_length_set(track, count);
    }
    groove_sequence_replace(track, 0, cells, count);
}

typedef struct { double peak, seconds; } result_t;

/* Play a pair for `bars` bars in each of `count` moods (the next mood is
 * written about 20 ms before its bar), stop, and let it ring for a second.
 * `path` NULL measures without writing. Volumes < 0 come from the table. */
static int render_pair(int drum, int melody, const char *path, unsigned bars, const unsigned *moods, unsigned count,
                       int drum_volume, int melody_volume, result_t *r)
{
    boot();
    if (drum >= 0) set_drums(drum, drum_volume);
    if (melody >= 0) set_melody(melody, moods[0], melody_volume, 1);
    ajb_wav_writer_t w;
    if (path && ajb_wav_open(&w, path, 2, FM1_SAMPLE_RATE)) return 1;
    const uint8_t start = 0xfa, stop = 0xfc;
    fm1_midi_post(&start, 1);
    double bar_blocks = 4.0 * 60.0 / BPM * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES;
    unsigned play = (unsigned)(bars * count * bar_blocks), tail = FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES, next = 1;
    double peak = 0;
    for (unsigned b = 0; b < play + tail; ++b) {
        if (melody >= 0 && next < count && b + 7U >= (unsigned)(bars * next * bar_blocks)) set_melody(melody, moods[next++], -1, 0);
        if (b == play) fm1_midi_post(&stop, 1);
        const float *out = sim_render();
        int16_t pcm[FM1_BLOCK_WORDS];
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
            double a = fabs(out[i]), v = out[i] * 32767.0;
            if (a > peak) peak = a;
            pcm[i] = (int16_t)(v > 32767.0 ? 32767 : v < -32768.0 ? -32768 : lrint(v));
        }
        if (path) ajb_wav_write(&w, pcm, FM1_BLOCK_FRAMES);
    }
    r->peak = peak > 0 ? 20.0 * log10(peak) : -999.0;
    r->seconds = path ? (double)w.frames / FM1_SAMPLE_RATE : 0.0;
    if (path) ajb_wav_close(&w);
    return 0;
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
static int render_page(unsigned p, const char *dir)
{
    static const unsigned happy = 0;
    const ajb_page_t *page = &ajb_factory_library.pages[p];
    int failed = 0;
    for (unsigned k = 0; k < AJB_SLOTS; ++k) {
        char a[64], b[64], path[512];
        slug(a, sizeof a, page->drums[k]->name);
        slug(b, sizeof b, page->melody[k]->name);
        snprintf(path, sizeof path, "%s/k%u_%s_%s.wav", dir, k + 1, a, b);
        int index = (int)(p * AJB_SLOTS + k);
        result_t r;
        failed |= render_pair(index, index, path, 8, &happy, 1, -1, -1, &r);
        printf("%-72s %.1fs  peak %6.2f dBFS\n", path, r.seconds, r.peak);
    }
    return failed;
}

/* The track volume (1..127) that brings a loop alone closest to `target` dBFS without passing it. */
static unsigned find_volume(unsigned side, int index, double target, double *reached)
{
    static const unsigned happy = 0;
    unsigned low = 1, high = 127;
    result_t r;
    while (low < high) {
        unsigned mid = (low + high + 1) / 2;
        render_pair(side ? -1 : index, side ? index : -1, NULL, 4, &happy, 1, (int)mid, (int)mid, &r);
        if (r.peak <= target) low = mid; else high = mid - 1;
    }
    render_pair(side ? -1 : index, side ? index : -1, NULL, 4, &happy, 1, (int)low, (int)low, &r);
    *reached = r.peak;
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

int main(int argc, char **argv)
{
    static const unsigned tour[MOODS] = {0, 1, 2, 3, 4, 5, 6};
    result_t r;
    if (argc >= 5 && !strcmp(argv[1], "pair")) {
        int drum = loop_index(argv[2]), melody = loop_index(argv[3]);
        unsigned mood = 0;
        while (argc > 6 && mood < MOODS && strcmp(argv[6], mood_names[mood])) ++mood;
        if (drum > -2 && melody > -2 && mood < MOODS) {
            int failed = render_pair(drum, melody, argv[4], argc > 5 ? (unsigned)atoi(argv[5]) : 8, &mood, 1, -1, -1, &r);
            if (!failed) printf("%s  %.1fs  peak %.2f dBFS\n", argv[4], r.seconds, r.peak);
            return failed;
        }
    }
    if (argc >= 4 && !strcmp(argv[1], "page") && atoi(argv[2]) >= 1 && atoi(argv[2]) <= (int)ajb_factory_library.page_count)
        return render_page((unsigned)atoi(argv[2]) - 1, argv[3]);
    if (argc >= 3 && !strcmp(argv[1], "all")) {
        int failed = 0;
        for (unsigned p = 0; p < ajb_factory_library.page_count; ++p) {
            char dir[400];
            snprintf(dir, sizeof dir, "%s/page%u", argv[2], p + 1);
            failed |= render_page(p, dir);
        }
        return failed;
    }
    if (argc >= 3 && !strcmp(argv[1], "tour")) {
        int failed = 0;
        for (unsigned p = 0; p < ajb_factory_library.page_count; ++p) {
            char path[512];
            snprintf(path, sizeof path, "%s/moods_page%u-key1.wav", argv[2], p + 1);
            failed |= render_pair((int)(p * 8), (int)(p * 8), path, 2, tour, MOODS, -1, -1, &r);
            printf("%-60s %.1fs  peak %6.2f dBFS\n", path, r.seconds, r.peak);
        }
        return failed;
    }
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
    if (argc >= 3 && !strcmp(argv[1], "levels"))
        return levels(argv[2], argc > 3 ? atof(argv[3]) : -9.0, argc > 4 ? atof(argv[4]) : -12.0);
    fprintf(stderr, "usage: ajstock pair <drum> <melody> <out.wav> [bars] [mood] | page <n> <dir> | all <dir>\n"
                    "       | tour <dir> | levels <volumes.inc> [drum dBFS] [melody dBFS]\n");
    return 2;
}
