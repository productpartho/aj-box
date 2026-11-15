/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box on FM1 Open's stock instrument (docs/s2-design.md), desktop build:
 * turns AJ Box loops into the stock sequencer's cells, sets kits and
 * instruments through the stock calls, and renders WAVs from FM1 Open's own
 * simulator. The device will make the same calls.
 *
 *   ajstock pair <drum> <melody> <out.wav> [bars] [split]
 *       loops as page.key (0 = none); split = 1 puts soft, normal and hard
 *       drum hits on three 808 tracks, 0 lets the loudest hit set the step
 *   ajstock page <n> <dir> [split]     the 8 same-key pairs of page n */
#include "ajbox_library.h"
#include "instrument.h"
#include "drum808.h"
#include "fm_patch.h"
#include "wav.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sim_boot(unsigned clear);
const float *sim_render(void);

/* Track use. Three 808 tracks only when hits are split by loudness. */
enum { T_DRUMS, T_DRUMS_SOFT, T_DRUMS_HARD, T_FM, T_PSG, T_PLUCK, T_BASS, T_SAMPLER };
enum { STEPS = GROOVE_STEPS, BPM = 110 };
enum { MOOD_HAPPY };
static const uint8_t mood_scale[5] = {0, 2, 4, 7, 9};   /* Happy; the other moods come with the full port */

static int degree_of(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static int degree_note(const ajb_melody_loop_t *m, int degree)
{
    if (m->notes && m->note_count)
        return 60 + 12 * m->octave + m->notes[degree % m->note_count] + 12 * (degree / m->note_count);
    return 60 + 12 * m->octave + mood_scale[degree % 5] + 12 * (degree / 5);
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
static unsigned melody_cells(const ajb_melody_loop_t *m, struct groove_step *cells)
{
    unsigned count = m->steps == 48 ? 48 : STEPS;
    memset(cells, 0, STEPS * sizeof *cells);
    for (unsigned s = 0; s < count; ++s) {
        unsigned pos = s % m->steps;
        if (m->riff) {
            int degree = degree_of(m->riff[pos]);
            if (degree < 0) continue;
            unsigned length = 1;
            while (pos + length < m->steps && m->riff[pos + length] == '-') ++length;
            put_note(cells, count, s, length, degree_note(m, degree), m->velocity);
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
        put_note(cells, count, s, m->rate, degree_note(m, chord[k % n_chord] + 5 * (int)(k / n_chord)), m->velocity);
    }
    return count;
}

/* A drum loop as cells. Unsplit: every hit on the step in one cell at the
 * loudest hit's velocity. Split: soft, normal and hard hits in three cells. */
static void drum_cells(const ajb_drum_loop_t *d, int split, struct groove_step cells[3][STEPS])
{
    static const uint8_t level[3] = {57, 99, 127};   /* 'o' 0.45, 'x' 0.78, 'X' 1.0 of full */
    memset(cells, 0, 3 * STEPS * sizeof cells[0][0]);
    for (unsigned s = 0; s < STEPS; ++s)
        for (unsigned p = 0; p < AJB_PARTS; ++p) {
            if (!d->parts[p]) continue;
            char c = d->parts[p][s % d->steps];
            int rank = c == 'o' ? 0 : c == 'x' ? 1 : c == 'X' ? 2 : -1;
            if (rank < 0) continue;
            struct groove_step *cell = &cells[split ? rank : 0][s];
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
static void apply_kit(unsigned track, const ajb_kit_t *kit)
{
    for (unsigned i = 0; kit && i < kit->count; ++i)
        tone_set(track, kit->edits[i].part, kit->edits[i].parameter, kit->edits[i].value);
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

static void boot(int split)
{
    static const uint8_t source[GROOVE_TRACKS] = {GROOVE_808, GROOVE_808, GROOVE_808, GROOVE_DX7, GROOVE_PSG,
                                                 GROOVE_PLUCK, GROOVE_303, GROOVE_SAMPLE};
    static const struct groove_step empty[STEPS];
    sim_boot(1);
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (groove_source_type(t) != source[t]) groove_source_set(t, source[t]);
        groove_sequence_replace(t, 0, empty, STEPS);
        if (groove_sequence_length(t) != STEPS) groove_sequence_length_set(t, STEPS);
    }
    (void)split;
    struct groove_view v;
    groove_get_view(0, 0, &v);
    groove_tempo_edit(BPM - (int)v.tempo);
}

typedef struct { double peak, seconds; } result_t;

static int render_pair(int drum, int melody, const char *path, unsigned bars, int split, result_t *r)
{
    const ajb_library_t *lib = &ajb_factory_library;
    boot(split);
    if (drum >= 0) {
        const ajb_page_t *page = &lib->pages[drum / AJB_SLOTS];
        const ajb_drum_loop_t *d = page->drums[drum % AJB_SLOTS];
        if (d && !d->sliced) {
            struct groove_step cells[3][STEPS];
            drum_cells(d, split, cells);
            for (unsigned t = 0; t < (split ? 3U : 1U); ++t) {
                apply_kit(T_DRUMS + t, lib->kits[page->kit]);
                groove_sequence_replace(T_DRUMS + t, 0, cells[t], STEPS);
            }
        }
    }
    if (melody >= 0) {
        const ajb_melody_loop_t *m = lib->pages[melody / AJB_SLOTS].melody[melody % AJB_SLOTS];
        struct groove_step cells[STEPS];
        unsigned track = melody_track(m->instrument), count = melody_cells(m, cells);
        apply_instrument(m->instrument);
        if (count != STEPS) groove_sequence_length_set(track, count);
        groove_sequence_replace(track, 0, cells, count);
    }
    ajb_wav_writer_t w;
    if (ajb_wav_open(&w, path, 2, FM1_SAMPLE_RATE)) return 1;
    const uint8_t start = 0xfa, stop = 0xfc;
    fm1_midi_post(&start, 1);
    unsigned play = (unsigned)(bars * 4.0 * 60.0 / BPM * FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES);
    unsigned tail = FM1_SAMPLE_RATE / FM1_BLOCK_FRAMES;
    double peak = 0;
    for (unsigned b = 0; b < play + tail; ++b) {
        if (b == play) fm1_midi_post(&stop, 1);
        const float *out = sim_render();
        int16_t pcm[FM1_BLOCK_WORDS];
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
            double a = fabs(out[i]), v = out[i] * 32767.0;
            if (a > peak) peak = a;
            pcm[i] = (int16_t)(v > 32767.0 ? 32767 : v < -32768.0 ? -32768 : lrint(v));
        }
        ajb_wav_write(&w, pcm, FM1_BLOCK_FRAMES);
    }
    r->peak = peak > 0 ? 20.0 * log10(peak) : -999.0;
    r->seconds = (double)w.frames / FM1_SAMPLE_RATE;
    ajb_wav_close(&w);
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

int main(int argc, char **argv)
{
    result_t r;
    if (argc >= 5 && !strcmp(argv[1], "pair")) {
        int drum = loop_index(argv[2]), melody = loop_index(argv[3]);
        if (drum > -2 && melody > -2) {
            int failed = render_pair(drum, melody, argv[4], argc > 5 ? (unsigned)atoi(argv[5]) : 8,
                                     argc > 6 && atoi(argv[6]), &r);
            if (!failed) printf("%s  %.1fs  peak %.2f dBFS\n", argv[4], r.seconds, r.peak);
            return failed;
        }
    }
    if (argc >= 4 && !strcmp(argv[1], "page")) {
        int page = atoi(argv[2]), failed = 0;
        if (page >= 1 && page <= (int)ajb_factory_library.page_count) {
            const ajb_page_t *p = &ajb_factory_library.pages[page - 1];
            for (unsigned k = 0; k < AJB_SLOTS; ++k) {
                char a[64], b[64], path[512];
                slug(a, sizeof a, p->drums[k]->name);
                slug(b, sizeof b, p->melody[k]->name);
                snprintf(path, sizeof path, "%s/k%u_%s_%s.wav", argv[3], k + 1, a, b);
                int index = (page - 1) * AJB_SLOTS + (int)k;
                failed |= render_pair(index, index, path, 8, argc > 4 && atoi(argv[4]), &r);
                printf("%-70s %.1fs  peak %6.2f dBFS\n", path, r.seconds, r.peak);
            }
            return failed;
        }
    }
    fprintf(stderr, "usage: ajstock pair <drum> <melody> <out.wav> [bars] [split] | page <n> <dir> [split]\n");
    return 2;
}
