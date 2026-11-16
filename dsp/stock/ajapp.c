/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's app logic on FM1 Open's stock instrument: see ajapp.h and
 * docs/s2-design.md. Loops become cells in the stock sequencer; a swap
 * rewrites a track while it plays, so the new loop carries on from the step
 * the beat is on (PRD "Playback rules"). */
#include "ajapp.h"
#include "instrument.h"
#include "drum808.h"
#include "fm_patch.h"
#include "effects.h"
#include "ajstore.h"
#include "fm1.h"
#include <string.h>

/* Track use (docs/s2-design.md). Tracks 7 and 8 stay empty. */
enum { T_DRUMS, T_SAMPLER, T_FM, T_PSG, T_PLUCK, T_BASS, T_USED };
enum { STEPS = GROOVE_STEPS, NONE = -1, FINISH_RING = 32 };

static const uint8_t mood_scale[AJAPP_MOODS][5] = {
    {0, 2, 4, 7, 9}, {0, 2, 4, 7, 9}, {0, 3, 5, 7, 10}, {0, 2, 4, 6, 8}, {0, 2, 5, 7, 10}, {0, 2, 3, 7, 8}, {0, 1, 4, 7, 8}};
static const uint8_t mood_root[AJAPP_MOODS] = {60, 65, 60, 60, 60, 60, 60};
/* Track volumes that put each loop on its level, measured by `ajstock levels`. */
static const uint8_t loop_volume[AJAPP_SIDES][48] = {
#include "volumes.inc"
};

static struct {
    const ajb_library_t *lib;
    int slot[AJAPP_SIDES];              /* playing loop, NONE when silent */
    int last[AJAPP_SIDES];              /* what PLAY/STOP brings back */
    uint8_t view_page[AJAPP_SIDES];
    uint8_t stopping[AJAPP_SIDES];      /* waiting for the next beat */
    uint8_t stop_step[AJAPP_SIDES];
    uint8_t muted[AJAPP_SIDES], fill, fast;
    uint8_t finishing, finish_step;     /* 1 waiting for the bar, 2 ringing */
    uint8_t mood, mood_wanted, mood_step, mood_waiting;
    int8_t octave;
    int kit, instrument;                /* PRESETS and ALGORITHM choices, NONE = the loop's own */
    uint8_t knob[AJAPP_KNOBS], effects;
    unsigned melody_track, bpm, dice;
    int debug_volume;
    ajb_fx_t fx;

    /* Everything a heart, a jam's start or power-on brings back: 32 bytes, saved as they are. */
    struct snapshot {
        uint8_t drum, melody;           /* page * 8 + slot, 255 = silent */
        uint8_t view_page[AJAPP_SIDES];
        uint8_t kit, instrument;        /* 255 = the loop's own */
        uint8_t effects, last_effect;
        uint8_t knob[AJAPP_KNOBS];
        uint8_t fx_amount[AJAPP_EFFECTS], fx_speed[AJAPP_EFFECTS];
        uint8_t bpm, mood, octave;      /* octave + 2 */
        uint8_t reserved[5];
    } memory;
    struct { struct snapshot heart[AJAPP_HEARTS]; uint8_t count, newest; uint8_t reserved[2]; } hearts;
    uint8_t heart_at;                   /* the heart SEQ is on, 0 = none yet */
    struct { struct snapshot start; uint16_t count, length; uint32_t event[AJAPP_JAM_EVENTS]; } jam;
    uint8_t has_jam, recording, replaying, quiet;
    uint32_t jam_began, replay_began, changed_at;
    uint16_t replay_at;
    uint8_t dirty;
    ajapp_parent_t parent;
    uint8_t parent_open, parent_row, parent_result, bluetooth_due;
    volatile uint8_t audio_step;        /* the playing step, for the sound interrupt */
    int16_t swap_to[AJAPP_SIDES];       /* next-bar swaps: the loop waiting, NONE when none */
    uint8_t swap_step[AJAPP_SIDES];
    struct { uint32_t off_at; uint8_t note; } chime[4];
    uint8_t chime_channel;
    struct { uint8_t on, step, from; } tail[8];     /* per track: a cell that lets a held note go (melody_put) */
} app;
_Static_assert(sizeof(struct snapshot) == 32, "snapshot layout");

/* ---- the stock instrument's position ---- */

static void position(unsigned *running, unsigned *step)
{
    struct groove_view v;
    groove_get_view(T_DRUMS, 0, &v);
    *running = v.playing;
    *step = v.play_step % STEPS;
}
static void transport(uint8_t status) { (void)fm1_midi_post(&status, 1); }
/* True once the playing step has reached `target`, counting from when it was set. */
static int reached(unsigned step, unsigned target) { return ((step - target) & (STEPS - 1)) < STEPS / 2; }

/* ---- loops as stock cells ---- */

static int degree_of(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static int degree_note(const ajb_melody_loop_t *m, int degree, unsigned mood)
{
    if (m->notes && m->note_count)      /* a fixed tune: as written, in every Mood */
        return 60 + 12 * (m->octave + app.octave) + m->notes[degree % m->note_count] + 12 * (degree / m->note_count);
    return mood_root[mood] + 12 * (m->octave + app.octave) + mood_scale[mood][degree % 5] + 12 * (degree / 5);
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
static const ajb_instrument_t *instrument_of(const ajb_melody_loop_t *m)
{
    return app.instrument != NONE ? app.lib->instruments[app.instrument] : m->instrument;
}
/* A melody loop as `count` cells (64, or 48 for a waltz). `speed` 2 is the
 * Fast key: every other step of the loop, twice as often. */
static unsigned melody_cells(const ajb_melody_loop_t *m, unsigned mood, unsigned speed, struct groove_step *cells)
{
    unsigned count = m->steps == 48 ? 48 : STEPS;
    /* The chip's 16 volume steps fall about 3 dB for every 8 of velocity, so lift it: 100 and up is full. */
    unsigned velocity = instrument_of(m)->kind != AJB_INSTRUMENT_PSG ? m->velocity : m->velocity + 27 > 127 ? 127 : m->velocity + 27U;
    memset(cells, 0, STEPS * sizeof *cells);
    for (unsigned s = 0; s < count; ++s) {
        unsigned pos = (s * speed) % m->steps;
        if (m->riff) {
            int degree = degree_of(m->riff[pos]);
            if (degree < 0) continue;
            unsigned length = 1;
            while (pos + length < m->steps && m->riff[pos + length] == '-') ++length;
            put_note(cells, count, s, (length + speed - 1) / speed, degree_note(m, degree, mood), velocity);
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
        put_note(cells, count, s, (m->rate + speed - 1) / speed,
                 degree_note(m, chord[k % n_chord] + 5 * (int)(k / n_chord), mood), velocity);
    }
    return count;
}
/* Step strings ('o' soft, 'x' normal, 'X' hard) as cells. A stock drum step
 * has one velocity for all its drums, so the loudest hit sets it. */
static void hit_cells(const char *const *rows, unsigned count, unsigned steps, struct groove_step *cells)
{
    static const uint8_t level[3] = {57, 99, 127};
    memset(cells, 0, STEPS * sizeof *cells);
    for (unsigned s = 0; s < STEPS; ++s)
        for (unsigned p = 0; p < count; ++p) {
            char c = rows[p] ? rows[p][s % steps] : '.';
            int rank = c == 'o' ? 0 : c == 'x' ? 1 : c == 'X' ? 2 : -1;
            if (rank < 0) continue;
            cells[s].drums |= (uint16_t)(1U << p);
            if (level[rank] > cells[s].velocity) cells[s].velocity = level[rank];
        }
}

/* ---- sounds and levels ---- */

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
static void apply_kit(const ajb_kit_t *kit)
{
    /* A kit is edits on top of the 808's defaults, so start from the defaults:
     * the last kit's edits must not linger. */
    uint8_t want[AJB_PARTS + 1][GROOVE_TONE_PARAMS];
    for (unsigned part = 0; part <= AJB_PARTS; ++part)
        for (unsigned p = 0; p < GROOVE_TONE_PARAMS; ++p) {
            const fm1_param_t *info = groove_tone_parameter(T_DRUMS, part, p);
            want[part][p] = info ? info->def : 0;
        }
    for (unsigned i = 0; kit && i < kit->count; ++i) {
        const ajb_kit_edit_t *e = &kit->edits[i];
        if (e->part > AJB_PARTS || e->parameter >= GROOVE_TONE_PARAMS) continue;
        want[e->part][e->parameter] = e->part == AJB_PARTS && e->parameter == 0 ? 110 : e->value;
    }
    /* Twice: switching a drum's Sound resets its other settings. */
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned part = 0; part <= AJB_PARTS; ++part)
            for (unsigned p = 0; p < groove_tone_count(T_DRUMS, part) && p < GROOVE_TONE_PARAMS; ++p)
                tone_set(T_DRUMS, part, p, want[part][p]);
}
static unsigned track_of(const ajb_instrument_t *in)
{
    return in->kind == AJB_INSTRUMENT_PSG ? T_PSG : in->kind == AJB_INSTRUMENT_PLUCK ? T_PLUCK
         : in->kind == AJB_INSTRUMENT_BASS ? T_BASS : T_FM;
}
static void apply_instrument(const ajb_instrument_t *in)
{
    unsigned track = track_of(in);
    if (in->kind == AJB_INSTRUMENT_FM) {
        uint8_t patch[156];
        groove_fm_program(patch, in->program);
        groove_fm_import(track, patch);
        return;
    }
    /* Every setting of the engine, not only the ones the instrument names: the
     * rest go back to the engine's own defaults. Leaving them alone let one
     * instrument's settings leak into the next on the same track (heard on the
     * FM-1, Nov 15, 2026: Crunchy Chip left the chip's noise on for Star Steps). */
    for (unsigned p = 0; p < GROOVE_TONE_PARAMS; ++p) {
        const fm1_param_t *info = groove_tone_parameter(track, 0, p);
        if (!info) continue;
        unsigned value = info->def;
        for (unsigned k = 0; k < in->knob_count; ++k) if (in->knobs[k].parameter == p) value = in->knobs[k].value;
        tone_set(track, 0, p, value);
    }
}
static void set_volume(unsigned track, unsigned volume)
{
    struct groove_view v;
    groove_get_view(track, 0, &v);
    groove_control_edit(track, 0, (int)volume - (int)v.volume, 0);
}
/* KNOB 4: the middle is even; either end takes the other side down to about a quarter. */
static unsigned balanced(unsigned side, unsigned volume)
{
    int tilt = (int)app.knob[AJAPP_KNOB_BALANCE] - 64;
    int cut = side == AJAPP_DRUMS ? (tilt > 0 ? tilt : 0) : (tilt < 0 ? -tilt : 0);
    return volume * (unsigned)(256 - 3 * cut) / 256U;
}
static void set_side_volume(unsigned side)
{
    if (app.slot[side] == NONE) return;
    unsigned volume = balanced(side, app.debug_volume >= 0 ? (unsigned)app.debug_volume : loop_volume[side][app.slot[side]]);
    if (side == AJAPP_DRUMS) { set_volume(T_DRUMS, volume); set_volume(T_SAMPLER, volume); }
    else set_volume(app.melody_track, volume);
}
static void set_mute(unsigned track, int muted)
{
    struct groove_view v;
    groove_get_view(track, 0, &v);
    if ((v.muted != 0) != (muted != 0)) groove_mix_toggle(track, 0);
}
/* Echo and Sparkle are the stock delay and reverb: a send on every track. */
static void set_sends(void)
{
    unsigned echo = app.effects & (1U << AJAPP_FX_ECHO) ? 4U + app.fx.amount[AJAPP_FX_ECHO] * 8U / 127U : 0;
    unsigned sparkle = app.effects & (1U << AJAPP_FX_SPARKLE) ? 3U + app.fx.amount[AJAPP_FX_SPARKLE] * 7U / 127U : 0;
    for (unsigned t = 0; t < T_USED; ++t) {
        uint8_t now[2];
        groove_sends_get(t, now);
        groove_sends_edit(t, 0, (int)sparkle - (int)now[0]);
        groove_sends_edit(t, 1, (int)echo - (int)now[1]);
    }
}

/* ---- writing loops into tracks ---- */

static const struct groove_step empty[STEPS];

/* Replace a track's cells from step `from` on, round to `from` again, leaving
 * the `keep` steps before `from` as they are: what is about to play stays. */
static void write_from(unsigned track, const struct groove_step *cells, unsigned count, unsigned from, unsigned keep)
{
    struct groove_step next[STEPS];
    memcpy(next, cells, count * sizeof *next);
    if (keep) {
        struct groove_step old[STEPS];
        groove_sequence_read(track, 0, old, count);
        for (unsigned i = 1; i <= keep; ++i) next[(from + count - i) % count] = old[(from + count - i) % count];
    }
    groove_sequence_replace(track, 0, next, count);
}
/* A held note is the same note in each cell, tied to the next (put_note). The
 * stock sequencer only lets a tied note go when another note cell arrives, so
 * taking the pattern away mid-note left it sounding for good (heard on the
 * FM-1, Nov 15, 2026, with Zelda's Lullaby). When the cell now playing is tied
 * and the next one is empty, write that note once more there, untied and as
 * short as a gate goes: the sequencer keeps it without striking it again and
 * lets it go a moment later. ajapp_tick takes the cell away once it has played. */
enum { TIE = 1U << 12, LET_GO = 1U << 5 };
static void melody_put(unsigned track, const struct groove_step *cells, unsigned count, unsigned from, unsigned keep)
{
    struct groove_step old[STEPS], put[STEPS];
    struct groove_view v;
    if (track >= 8) return;
    groove_sequence_read(track, 0, old, STEPS);
    groove_get_view(track, 0, &v);
    /* A let-go cell from a moment ago that has not played yet (two presses inside
     * one 16th): the note it is for is still held, so it has to survive this write. */
    const unsigned waiting = app.tail[track].on && v.playing && v.play_step == app.tail[track].from &&
                             old[app.tail[track].step].velocity && old[app.tail[track].step].drums == LET_GO;
    const struct groove_step kept = old[app.tail[track].step];
    write_from(track, cells, count, from, keep);
    groove_get_view(track, 0, &v);
    if (!v.playing) return;                     /* stopped: the sequencer has let everything go */
    unsigned length = v.sequence_length ? v.sequence_length : STEPS, p = v.play_step % length, q = (p + 1U) % length;
    groove_sequence_read(track, 0, put, STEPS);
    if (waiting) {
        /* A new note on that step lets the old one go by itself; otherwise put the cell back. */
        if (!put[app.tail[track].step].velocity) groove_sequence_replace(track, app.tail[track].step, &kept, 1);
        else app.tail[track].on = 0;
        return;
    }
    if (!old[p].velocity || !(old[p].drums & TIE) || put[q].velocity) return;
    struct groove_step end = old[p];
    end.drums = LET_GO;
    groove_sequence_replace(track, q, &end, 1);
    app.tail[track].on = 1; app.tail[track].step = (uint8_t)q; app.tail[track].from = (uint8_t)p;
}
static void tails_tick(void)
{
    for (unsigned t = 0; t < 8; ++t) {
        if (!app.tail[t].on) continue;
        struct groove_view v;
        groove_get_view(t, 0, &v);
        if (v.playing && (v.play_step == app.tail[t].from || v.play_step == app.tail[t].step)) continue;
        struct groove_step cell;
        groove_sequence_read(t, app.tail[t].step, &cell, 1);
        if (cell.velocity && cell.drums == LET_GO) groove_sequence_replace(t, app.tail[t].step, empty, 1);
        app.tail[t].on = 0;
    }
}

static const ajb_drum_loop_t *drum_loop(int index) { return app.lib->pages[index / AJB_SLOTS].drums[index % AJB_SLOTS]; }
static const ajb_melody_loop_t *melody_loop(int index) { return app.lib->pages[index / AJB_SLOTS].melody[index % AJB_SLOTS]; }

static void write_drums(void)
{
    struct groove_step cells[STEPS];
    if (app.slot[AJAPP_DRUMS] == NONE) {
        groove_sequence_replace(T_DRUMS, 0, empty, STEPS);
        groove_sequence_replace(T_SAMPLER, 0, empty, STEPS);
        return;
    }
    const ajb_drum_loop_t *d = app.fill && app.lib->fill ? app.lib->fill : drum_loop(app.slot[AJAPP_DRUMS]);
    hit_cells(d->parts, AJB_PARTS, d->steps, cells);
    groove_sequence_replace(T_DRUMS, 0, cells, STEPS);
    hit_cells(d->pads, AJB_PADS, d->steps, cells);
    groove_sequence_replace(T_SAMPLER, 0, cells, STEPS);
}
static void set_drum_sound(void)
{
    if (app.slot[AJAPP_DRUMS] == NONE) return;
    unsigned kit = app.kit != NONE ? (unsigned)app.kit : app.lib->pages[app.slot[AJAPP_DRUMS] / AJB_SLOTS].kit;
    if (kit < app.lib->kit_count) apply_kit(app.lib->kits[kit]);
    set_side_volume(AJAPP_DRUMS);
}
/* `from`/`keep` as in write_from: Mood lands on the bar, everything else at once. */
static void write_melody(unsigned from, unsigned keep)
{
    struct groove_step cells[STEPS];
    if (app.slot[AJAPP_MELODY] == NONE) {
        melody_put(app.melody_track, empty, STEPS, 0, 0);
        return;
    }
    const ajb_melody_loop_t *m = melody_loop(app.slot[AJAPP_MELODY]);
    unsigned count = melody_cells(m, app.mood, app.fast ? 2 : 1, cells);
    if (groove_sequence_length(app.melody_track) != count) groove_sequence_length_set(app.melody_track, count);
    melody_put(app.melody_track, cells, count, from % count, keep);
}
static void set_melody_sound(void)
{
    if (app.slot[AJAPP_MELODY] == NONE) return;
    const ajb_instrument_t *in = instrument_of(melody_loop(app.slot[AJAPP_MELODY]));
    unsigned track = track_of(in);
    if (track != app.melody_track) {
        melody_put(app.melody_track, empty, STEPS, 0, 0);
        app.melody_track = track;
    }
    apply_instrument(in);
    set_side_volume(AJAPP_MELODY);
    set_mute(app.melody_track, app.muted[AJAPP_MELODY]);
}

static void start_side(unsigned side, int index)
{
    unsigned running, step;
    position(&running, &step);
    app.slot[side] = app.last[side] = index;
    app.stopping[side] = 0;
    app.swap_to[side] = NONE;
    if (app.finishing) { app.finishing = 0; }
    if (side == AJAPP_DRUMS) { set_drum_sound(); write_drums(); }
    else {
        app.instrument = NONE;          /* a new melody loop goes back to its own instrument */
        set_melody_sound();
        write_melody(0, 0);
    }
    if (!running) transport(0xfa);
}
static void clear_side(unsigned side)
{
    app.slot[side] = NONE;
    app.swap_to[side] = NONE;
    app.stopping[side] = 0;
    if (side == AJAPP_DRUMS) write_drums(); else write_melody(0, 0);
    if (app.slot[AJAPP_DRUMS] == NONE && app.slot[AJAPP_MELODY] == NONE) transport(0xfc);
}

/* ---- controls ---- */

/* Every control is one of these, so a jam can record and replay them: 10 ms
 * ticks since the jam began in the top 14 bits, then type, a and b. */
enum { C_PICK, C_PAGE, C_HOLD, C_FINISH, C_PLAY_STOP, C_HOME, C_EFFECT, C_KNOB, C_BPM, C_KIT, C_INSTRUMENT, C_OCTAVE,
       C_MOOD, C_HEART };
static void note(unsigned type, unsigned a, unsigned b)
{
    if (app.quiet) return;
    app.changed_at = fm1_millis();
    app.dirty = 1;
    if (!app.recording || app.replaying || app.jam.count >= AJAPP_JAM_EVENTS) return;
    uint32_t tick = (fm1_millis() - app.jam_began) / 10U;
    if (tick > 16383U) tick = 16383U;
    app.jam.event[app.jam.count++] = tick << 18 | (type & 31U) << 13 | (a & 31U) << 8 | (b & 255U);
}

void ajapp_pick(unsigned side, unsigned index)
{
    if (!app.lib || side >= AJAPP_SIDES || index >= app.lib->page_count * AJB_SLOTS) return;
    note(C_PICK, side, index);
    unsigned running, step;
    position(&running, &step);
    if (running && app.slot[side] == (int)index && !app.stopping[side]) {
        app.stopping[side] = 1;         /* the playing key again: stop on the next beat */
        app.stop_step[side] = (uint8_t)((step / 4U + 1U) * 4U % STEPS);
        app.swap_to[side] = NONE;
        return;
    }
    if (running && app.parent.swap_on_bar && !app.finishing) {   /* PRD "Parent mode": wait for the next bar */
        app.swap_to[side] = (int16_t)index;
        app.swap_step[side] = (uint8_t)((step / 16U + 1U) * 16U % STEPS);
        return;
    }
    start_side(side, (int)index);
}
void ajapp_press(unsigned side, unsigned slot)
{
    if (side < AJAPP_SIDES && slot < AJB_SLOTS) ajapp_pick(side, app.view_page[side] * AJB_SLOTS + slot);
}
void ajapp_page_step(unsigned side, int delta)
{
    if (!app.lib || side >= AJAPP_SIDES) return;
    int pages = (int)app.lib->page_count;
    app.view_page[side] = (uint8_t)(((int)app.view_page[side] + delta % pages + pages) % pages);
    note(C_PAGE, side, app.view_page[side]);
}
void ajapp_dice(unsigned side)
{
    if (!app.lib || side >= AJAPP_SIDES) return;
    unsigned total = app.lib->page_count * AJB_SLOTS, index;
    do { app.dice = app.dice * 1664525U + 1013904223U; index = (app.dice >> 8) % total; } while ((int)index == app.slot[side]);
    app.view_page[side] = (uint8_t)(index / AJB_SLOTS);
    note(C_PAGE, side, app.view_page[side]);
    note(C_PICK, side, index);          /* a replayed jam plays the same surprise */
    if (app.stopping[side]) app.stopping[side] = 0;
    start_side(side, (int)index);
}
void ajapp_hold(unsigned hold, int down)
{
    unsigned on = down != 0;
    if (hold < AJAPP_HOLDS && hold != AJAPP_HOLD_STUTTER) note(C_HOLD, hold, on);
    switch (hold) {
    case AJAPP_HOLD_FILL:
        if (app.fill != on) { app.fill = (uint8_t)on; write_drums(); }
        break;
    case AJAPP_HOLD_FAST:
        if (app.fast != on) { app.fast = (uint8_t)on; write_melody(0, 0); }
        break;
    case AJAPP_HOLD_MUTE_DRUMS:
        app.muted[AJAPP_DRUMS] = (uint8_t)on;
        set_mute(T_DRUMS, on); set_mute(T_SAMPLER, on);
        break;
    case AJAPP_HOLD_MUTE_MELODY:
        app.muted[AJAPP_MELODY] = (uint8_t)on;
        set_mute(app.melody_track, on);
        break;
    case AJAPP_HOLD_STUTTER: ajapp_effect(AJAPP_FX_STUTTER, down); break;
    default: break;
    }
}
/* Finish (PRD "Playback rules" 7): on the next bar a crash and the melody's
 * last chord, written ahead so they land exactly, then the clock stops. */
void ajapp_finish(void)
{
    unsigned running, step;
    position(&running, &step);
    if (!running || app.finishing || (app.slot[AJAPP_DRUMS] == NONE && app.slot[AJAPP_MELODY] == NONE)) return;
    note(C_FINISH, 0, 0);
    app.swap_to[AJAPP_DRUMS] = app.swap_to[AJAPP_MELODY] = NONE;
    unsigned bar = (step / 16U + 1U) * 16U % STEPS, keep = (bar - step) & (STEPS - 1);
    struct groove_step cells[STEPS];
    memset(cells, 0, sizeof cells);
    cells[bar].drums = (uint16_t)((1U << D8_CY) | (1U << D8_BD));
    cells[bar].velocity = 127;
    write_from(T_DRUMS, cells, STEPS, bar, keep);
    write_from(T_SAMPLER, empty, STEPS, bar, keep);
    if (app.slot[AJAPP_MELODY] != NONE) {
        const ajb_melody_loop_t *m = melody_loop(app.slot[AJAPP_MELODY]);
        unsigned count = groove_sequence_length(app.melody_track);
        memset(cells, 0, sizeof cells);
        if (bar < count)
            for (unsigned i = 0; i < 8 && bar + i < count; ++i) {       /* the loop's first note, held half a bar */
                cells[bar + i].note = (uint8_t)degree_note(m, m->riff ? 0 : degree_of(m->chord[0][0]), app.mood);
                cells[bar + i].velocity = 100;
                cells[bar + i].drums = (uint16_t)((15U << 5) | (i < 7 ? 1U << 12 : 0U));
            }
        melody_put(app.melody_track, cells, count, bar % count, keep < count ? keep : 0);
    }
    app.finishing = 1;
    app.finish_step = (uint8_t)bar;
    app.stopping[AJAPP_DRUMS] = app.stopping[AJAPP_MELODY] = 0;
}
void ajapp_play_stop(void)
{
    note(C_PLAY_STOP, 0, 0);
    unsigned running, step;
    position(&running, &step);
    if (running) {
        app.finishing = 0;
        app.slot[AJAPP_DRUMS] = app.slot[AJAPP_MELODY] = NONE;
        app.stopping[AJAPP_DRUMS] = app.stopping[AJAPP_MELODY] = 0;
        app.swap_to[AJAPP_DRUMS] = app.swap_to[AJAPP_MELODY] = NONE;
        write_drums(); write_melody(0, 0);
        transport(0xfc);
        return;
    }
    for (unsigned side = 0; side < AJAPP_SIDES; ++side)
        if (app.last[side] != NONE) start_side(side, app.last[side]);
}
void ajapp_effect(unsigned effect, int on)
{
    if (effect >= AJAPP_EFFECTS) return;
    note(C_EFFECT, effect, on != 0);
    unsigned running, step;
    position(&running, &step);
    app.effects = (uint8_t)(on ? app.effects | (1U << effect) : app.effects & ~(1U << effect));
    if (effect == AJAPP_FX_ECHO || effect == AJAPP_FX_SPARKLE) { if (on) app.fx.last = (uint8_t)effect; set_sends(); }
    else ajb_fx_switch(&app.fx, effect, on, app.bpm, (float)step / 4.0f);
}
void ajapp_home(void)
{
    note(C_HOME, 0, 0);
    ++app.quiet;
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) ajapp_effect(k, 0);
    --app.quiet;
    app.knob[AJAPP_KNOB_SWOOSH] = 64;
    ajb_fx_swoosh(&app.fx, 64);
}
void ajapp_knob(unsigned knob, unsigned value)
{
    if (knob >= AJAPP_KNOBS) return;
    note(C_KNOB, knob, value > 127 ? 127 : value);
    app.knob[knob] = (uint8_t)(value > 127 ? 127 : value);
    if (knob == AJAPP_KNOB_SWOOSH) ajb_fx_swoosh(&app.fx, app.knob[knob]);
    else if (knob == AJAPP_KNOB_BALANCE) { set_side_volume(AJAPP_DRUMS); set_side_volume(AJAPP_MELODY); }
    else { ajb_fx_knob(&app.fx, knob, app.knob[knob]); set_sends(); }
}
void ajapp_set_bpm(unsigned bpm)
{
    bpm = bpm < AJAPP_BPM_MIN ? AJAPP_BPM_MIN : bpm > AJAPP_BPM_MAX ? AJAPP_BPM_MAX : bpm;
    struct groove_view v;
    groove_get_view(T_DRUMS, 0, &v);
    groove_tempo_edit((int)bpm - (int)v.tempo);
    app.bpm = bpm;
    note(C_BPM, 0, bpm);
}
void ajapp_set_kit(unsigned kit)
{
    if (!app.lib || kit >= app.lib->kit_count) return;
    app.kit = (int)kit;
    note(C_KIT, 0, kit);
    set_drum_sound();
}
void ajapp_set_instrument(unsigned instrument)
{
    if (!app.lib || instrument >= app.lib->instrument_count) return;
    app.instrument = (int)instrument;
    note(C_INSTRUMENT, 0, instrument);
    set_melody_sound();
    write_melody(0, 0);
}
void ajapp_set_octave(int octave)
{
    app.octave = (int8_t)(octave < -2 ? -2 : octave > 2 ? 2 : octave);
    note(C_OCTAVE, 0, (unsigned)(app.octave + 2));
    write_melody(0, 0);
}
/* Mood lands on the next bar line while playing: the rest of this bar keeps its notes. */
void ajapp_set_mood(unsigned mood)
{
    if (mood >= AJAPP_MOODS) return;
    note(C_MOOD, 0, mood);
    unsigned running, step;
    position(&running, &step);
    app.mood_wanted = (uint8_t)mood;
    if (!running || app.slot[AJAPP_MELODY] == NONE) { app.mood = (uint8_t)mood; app.mood_waiting = 0; write_melody(0, 0); return; }
    unsigned bar = (step / 16U + 1U) * 16U % STEPS;
    app.mood = (uint8_t)mood;
    write_melody(bar, (bar - step) & (STEPS - 1));
    app.mood_step = (uint8_t)bar;
    app.mood_waiting = 1;
}

/* ---- favourites, the jam and memory ---- */

static void snapshot_take(struct snapshot *s)
{
    memset(s, 0, sizeof *s);
    s->drum = (uint8_t)(app.slot[AJAPP_DRUMS] == NONE ? 255 : app.slot[AJAPP_DRUMS]);
    s->melody = (uint8_t)(app.slot[AJAPP_MELODY] == NONE ? 255 : app.slot[AJAPP_MELODY]);
    if (s->drum == 255 && s->melody == 255) {          /* stopped: remember the pair PLAY/STOP would bring back */
        s->drum = (uint8_t)(app.last[AJAPP_DRUMS] == NONE ? 255 : app.last[AJAPP_DRUMS]);
        s->melody = (uint8_t)(app.last[AJAPP_MELODY] == NONE ? 255 : app.last[AJAPP_MELODY]);
    }
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) s->view_page[side] = app.view_page[side];
    s->kit = (uint8_t)(app.kit == NONE ? 255 : app.kit);
    s->instrument = (uint8_t)(app.instrument == NONE ? 255 : app.instrument);
    s->effects = app.effects & (uint8_t)~(1U << AJAPP_FX_STUTTER);      /* Stutter works only while held */
    s->last_effect = app.fx.last;
    memcpy(s->knob, app.knob, sizeof s->knob);
    memcpy(s->fx_amount, app.fx.amount, sizeof s->fx_amount);
    memcpy(s->fx_speed, app.fx.speed, sizeof s->fx_speed);
    s->bpm = (uint8_t)app.bpm; s->mood = app.mood; s->octave = (uint8_t)(app.octave + 2);
}
/* Saved data is checked before it is used: every index has to be in range. */
static int snapshot_valid(const struct snapshot *s)
{
    unsigned loops = app.lib->page_count * AJB_SLOTS;
    if ((s->drum != 255 && s->drum >= loops) || (s->melody != 255 && s->melody >= loops)) return 0;
    if (s->view_page[0] >= app.lib->page_count || s->view_page[1] >= app.lib->page_count) return 0;
    if ((s->kit != 255 && s->kit >= app.lib->kit_count) || (s->instrument != 255 && s->instrument >= app.lib->instrument_count)) return 0;
    if (s->effects >= 1U << AJAPP_EFFECTS || s->last_effect >= AJAPP_EFFECTS || s->mood >= AJAPP_MOODS || s->octave > 4) return 0;
    if (s->bpm < AJAPP_BPM_MIN || s->bpm > AJAPP_BPM_MAX) return 0;
    for (unsigned k = 0; k < AJAPP_KNOBS; ++k) if (s->knob[k] > 127) return 0;
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) if (s->fx_amount[k] > 127 || s->fx_speed[k] > 127) return 0;
    return 1;
}
/* Bring a snapshot back. `play` starts its pair; otherwise the pair waits for PLAY/STOP. */
static void fx_speed_apply(void);
static void snapshot_apply(const struct snapshot *s, int play)
{
    if (!snapshot_valid(s)) return;
    ++app.quiet;
    unsigned running, step;
    position(&running, &step);
    if (!play && running) ajapp_play_stop();
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) app.view_page[side] = s->view_page[side];
    app.kit = s->kit == 255 ? NONE : s->kit;
    memcpy(app.fx.amount, s->fx_amount, sizeof s->fx_amount);
    memcpy(app.fx.speed, s->fx_speed, sizeof s->fx_speed);
    fx_speed_apply();                   /* the parent setting wins over speeds saved before it existed */
    for (unsigned k = 0; k < AJAPP_KNOBS; ++k) ajapp_knob(k, s->knob[k]);
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) ajapp_effect(k, (s->effects >> k) & 1U);
    app.fx.last = s->last_effect;
    ajapp_set_bpm(s->bpm);
    app.octave = (int8_t)((int)s->octave - 2);
    app.mood = app.mood_wanted = s->mood; app.mood_waiting = 0;
    app.last[AJAPP_DRUMS] = s->drum == 255 ? NONE : s->drum;
    app.last[AJAPP_MELODY] = s->melody == 255 ? NONE : s->melody;
    if (play) {
        for (unsigned side = 0; side < AJAPP_SIDES; ++side) {
            int want = app.last[side];
            if (want == NONE && app.slot[side] != NONE) clear_side(side);
            else if (want != NONE) start_side(side, want);
        }
        if (s->instrument != 255) ajapp_set_instrument(s->instrument);
    } else
        app.instrument = s->instrument == 255 ? NONE : s->instrument;
    --app.quiet;
}

unsigned ajapp_save_heart(void)
{
    if (!app.lib) return 0;
    unsigned index = app.hearts.count < AJAPP_HEARTS ? app.hearts.count : (app.hearts.newest + 1U) % AJAPP_HEARTS;
    __typeof__(app.hearts) next = app.hearts;          /* a new heart replaces the oldest */
    snapshot_take(&next.heart[index]);
    if (next.count < AJAPP_HEARTS) ++next.count;
    next.newest = (uint8_t)index;
    if (ajstore_save(AJSTORE_HEARTS, &next, sizeof next)) return 0;     /* not saved: the heart stays empty */
    app.hearts = next;
    app.heart_at = (uint8_t)(index + 1U);
    return index + 1U;
}
unsigned ajapp_next_heart(void)
{
    if (!app.lib || !app.hearts.count) return 0;
    unsigned index = app.heart_at % app.hearts.count;
    app.heart_at = (uint8_t)(index + 1U);
    note(C_HEART, 0, index);
    snapshot_apply(&app.hearts.heart[index], 1);
    return index + 1U;
}
int ajapp_record(void)
{
    if (!app.lib) return -1;
    if (!app.recording) {
        if (app.replaying) ajapp_replay();
        snapshot_take(&app.jam.start);
        unsigned running, step;
        position(&running, &step);
        if (!running) app.jam.start.drum = app.jam.start.melody = 255;  /* a jam that starts in silence */
        app.jam.count = app.jam.length = 0;
        app.jam_began = fm1_millis();
        app.recording = 1;
        app.has_jam = 0;
        return 1;
    }
    app.recording = 0;
    uint32_t ticks = (fm1_millis() - app.jam_began) / 10U;
    app.jam.length = (uint16_t)(ticks > AJAPP_JAM_SECONDS * 100U ? AJAPP_JAM_SECONDS * 100U : ticks);
    if (ajstore_save(AJSTORE_JAM, &app.jam, sizeof app.jam)) return -1;  /* not saved: no jam to replay */
    app.has_jam = 1;
    return 0;
}
static void dispatch(unsigned type, unsigned a, unsigned b)
{
    switch (type) {
    case C_PICK: ajapp_pick(a, b); break;
    case C_PAGE: if (a < AJAPP_SIDES && b < app.lib->page_count) app.view_page[a] = (uint8_t)b; break;
    case C_HOLD: ajapp_hold(a, (int)b); break;
    case C_FINISH: ajapp_finish(); break;
    case C_PLAY_STOP: ajapp_play_stop(); break;
    case C_HOME: ajapp_home(); break;
    case C_EFFECT: ajapp_effect(a, (int)b); break;
    case C_KNOB: ajapp_knob(a, b); break;
    case C_BPM: ajapp_set_bpm(b); break;
    case C_KIT: ajapp_set_kit(b); break;
    case C_INSTRUMENT: ajapp_set_instrument(b); break;
    case C_OCTAVE: ajapp_set_octave((int)b - 2); break;
    case C_MOOD: ajapp_set_mood(b); break;
    case C_HEART: if (b < app.hearts.count) snapshot_apply(&app.hearts.heart[b], 1); break;
    default: break;
    }
}
void ajapp_replay(void)
{
    if (!app.lib) return;
    if (app.replaying) { app.replaying = 0; return; }
    if (!app.has_jam || app.recording) return;
    unsigned running, step;
    position(&running, &step);
    ++app.quiet;
    if (running) ajapp_play_stop();
    --app.quiet;
    snapshot_apply(&app.jam.start, 1);
    /* The stop above has not reached the instrument yet, so the loops just put back
     * did not start the clock themselves: start it, from the top. */
    if (running && (app.slot[AJAPP_DRUMS] != NONE || app.slot[AJAPP_MELODY] != NONE)) transport(0xfa);
    app.replaying = 1;
    app.replay_at = 0;
    app.replay_began = fm1_millis();
}
int ajapp_save_memory(void)
{
    if (!app.lib) return -1;
    struct snapshot s;
    snapshot_take(&s);
    app.dirty = 0;
    if (!memcmp(&s, &app.memory, sizeof s)) return 0;      /* nothing new: spare the flash */
    if (ajstore_save(AJSTORE_MEMORY, &s, sizeof s)) return -1;
    app.memory = s;
    return 0;
}
/* Bluetooth follows the parent setting: music and MIDI on, or everything off. The radio can be busy; then try again. */
static void bluetooth_apply(void)
{
    app.bluetooth_due = fm1_bluetooth_configure(app.parent.bluetooth ? FM1_BT_AUDIO | FM1_BT_MIDI : 0) == -2;
}
void ajapp_parent_get(ajapp_parent_t *settings) { *settings = app.parent; }
/* How fast the effects go is a parent setting (it was KNOB 3 until Nov 15, 2026).
 * Normal is each effect's own speed: Stutter an 8th note (PRD "Effects"), the
 * rest the middle. Slow and Fast move them all to one end. */
static void fx_speed_apply(void)
{
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k)
        app.fx.speed[k] = app.parent.fx_speed == AJAPP_SPEED_SLOW ? 16 : app.parent.fx_speed == AJAPP_SPEED_FAST ? (k == AJAPP_FX_STUTTER ? 64 : 112)
                        : k == AJAPP_FX_STUTTER ? 20 : 64;
    app.knob[AJAPP_KNOB_SPEED] = app.fx.speed[app.fx.last];
}
int ajapp_parent_set(const ajapp_parent_t *settings)
{
    ajapp_parent_t next = *settings;
    if (next.default_bpm < AJAPP_BPM_MIN || next.default_bpm > AJAPP_BPM_MAX) return -1;
    next.swap_on_bar = next.swap_on_bar != 0; next.bluetooth = next.bluetooth != 0;
    if (next.fx_speed >= AJAPP_SPEEDS) return -1;
    if (ajstore_save(AJSTORE_PARENT, &next, sizeof next)) return -1;    /* not saved: the old setting stands */
    app.parent = next;
    bluetooth_apply();
    fx_speed_apply();
    return 0;
}
void ajapp_parent_open(void) { app.parent_open = 1; app.parent_row = 0; app.parent_result = AJAPP_PARENT_UNCHANGED; }
void ajapp_parent_close(void) { app.parent_open = 0; }
void ajapp_parent_row(int delta)
{
    if (!app.parent_open) return;
    app.parent_row = (uint8_t)(((int)app.parent_row + delta % AJAPP_PARENT_ROWS + AJAPP_PARENT_ROWS) % AJAPP_PARENT_ROWS);
}
int ajapp_parent_change(int delta)
{
    if (!app.parent_open || !delta) return -1;
    ajapp_parent_t next = app.parent;
    if (app.parent_row == AJAPP_PARENT_TEMPO) {
        int bpm = (int)next.default_bpm + (delta > 0 ? 5 : -5);
        next.default_bpm = (uint8_t)(bpm < AJAPP_BPM_MIN ? AJAPP_BPM_MIN : bpm > AJAPP_BPM_MAX ? AJAPP_BPM_MAX : bpm);
    } else if (app.parent_row == AJAPP_PARENT_SWAP) next.swap_on_bar = !next.swap_on_bar;
    else if (app.parent_row == AJAPP_PARENT_BLUETOOTH) next.bluetooth = !next.bluetooth;
    else {                              /* Slow, Normal, Fast, stopping at the ends */
        static const uint8_t order[AJAPP_SPEEDS] = {AJAPP_SPEED_SLOW, AJAPP_SPEED_NORMAL, AJAPP_SPEED_FAST};
        int at = next.fx_speed == AJAPP_SPEED_SLOW ? 0 : next.fx_speed == AJAPP_SPEED_FAST ? 2 : 1;
        at += delta > 0 ? 1 : -1;
        next.fx_speed = order[at < 0 ? 0 : at > 2 ? 2 : at];
    }
    int failed = ajapp_parent_set(&next);
    app.parent_result = failed ? AJAPP_PARENT_NOT_SAVED : AJAPP_PARENT_SAVED;
    if (!failed && app.parent_row == AJAPP_PARENT_TEMPO) ajapp_set_bpm(next.default_bpm);    /* hear the new speed now */
    return failed ? -1 : 0;
}
/* Hello: C5 E5 G5 C6 on the FM track, about a second. */
void ajapp_chime(void)
{
    static const uint8_t notes[4] = {72, 76, 79, 84};
    if (!app.lib) return;
    app.chime_channel = (uint8_t)(groove_midi_input_channel(T_FM) & 15U);
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t on[3] = {(uint8_t)(0x90 | app.chime_channel), notes[i], (uint8_t)(50 + 10 * i)};
        (void)fm1_midi_post(on, 3);
        app.chime[i].note = notes[i];
        app.chime[i].off_at = fm1_millis() + 700U + 60U * i;
    }
}

void ajapp_tick(void)
{
    if (!app.lib) return;
    uint32_t ms = fm1_millis();
    tails_tick();
    for (unsigned i = 0; i < 4; ++i)
        if (app.chime[i].note && (int32_t)(ms - app.chime[i].off_at) >= 0) {
            uint8_t off[3] = {(uint8_t)(0x80 | app.chime_channel), app.chime[i].note, 0};
            (void)fm1_midi_post(off, 3);
            app.chime[i].note = 0;
        }
    if (app.recording && ms - app.jam_began >= AJAPP_JAM_SECONDS * 1000U) (void)ajapp_record();   /* 2 minutes: stop and keep */
    if (app.replaying) {
        uint32_t tick = (ms - app.replay_began) / 10U;
        while (app.replaying && app.replay_at < app.jam.count && app.jam.event[app.replay_at] >> 18 <= tick) {
            uint32_t e = app.jam.event[app.replay_at++];
            dispatch((e >> 13) & 31U, (e >> 8) & 31U, e & 255U);
        }
        if (app.replaying && tick >= app.jam.length) app.replaying = 0;
    }
    /* PRD "Playback rules" 10: save what power-on brings back 30 s after the last change. */
    /* Rule 10. Writing flash cuts the sound for a moment (heard on the FM-1, Nov 15, 2026), so what
     * power-on brings back is saved only while nothing is playing: 5 s after the music stops or
     * after the last change made in silence. */
    if (app.dirty && !app.recording && !app.replaying && ms - app.changed_at >= 5000U) {
        unsigned running, step;
        position(&running, &step);
        if (!running) (void)ajapp_save_memory();
    }
    unsigned running, step;
    position(&running, &step);
    app.audio_step = (uint8_t)step;
    for (unsigned side = 0; side < AJAPP_SIDES; ++side)
        if (app.stopping[side] && (!running || reached(step, app.stop_step[side]))) clear_side(side);
    for (unsigned side = 0; side < AJAPP_SIDES; ++side)
        if (app.swap_to[side] != NONE && (!running || reached(step, app.swap_step[side]))) start_side(side, app.swap_to[side]);
    if (app.bluetooth_due) bluetooth_apply();
    if (app.mood_waiting && (!running || reached(step, app.mood_step))) {
        app.mood_waiting = 0;
        write_melody(0, 0);             /* the bar has turned: the kept steps take the new mood too */
    }
    if (app.finishing == 1 && running && reached(step, app.finish_step)) app.finishing = 2;
    if (app.finishing == 2 && (!running || reached(step, (app.finish_step + FINISH_RING) % STEPS))) {
        app.finishing = 0;
        app.slot[AJAPP_DRUMS] = app.slot[AJAPP_MELODY] = NONE;
        write_drums(); write_melody(0, 0);
        transport(0xfc);
    }
}

/* The finished mix: Wobble, Wah, Crunch, Stutter and the Swoosh filter. */
/* Called from the sound interrupt: on the FM-1 it must not call anything kept in
 * flash, so the step comes from ajapp_tick, not from the instrument. */
FM1_AUDIO_CODE void ajapp_audio(int32_t *block, unsigned frames)
{
    static int32_t left[AJB_BLOCK], right[AJB_BLOCK], mono[AJB_BLOCK];
    if (frames != AJB_BLOCK || !app.lib) return;
    const unsigned step = app.audio_step;
    for (unsigned i = 0; i < AJB_BLOCK; ++i) {         /* the effects work a bit above Q23 */
        left[i] = block[2 * i] * 2; right[i] = block[2 * i + 1] * 2;
        mono[i] = (left[i] >> 1) + (right[i] >> 1);
    }
    ajb_fx_process(&app.fx, mono, left, right, app.bpm, (float)step / 4.0f);
    for (unsigned i = 0; i < AJB_BLOCK; ++i) {
        int32_t l = left[i] / 2, r = right[i] / 2;
        block[2 * i] = l > 8388607 ? 8388607 : l < -8388608 ? -8388608 : l;
        block[2 * i + 1] = r > 8388607 ? 8388607 : r < -8388608 ? -8388608 : r;
    }
}

void ajapp_init(const ajb_library_t *library)
{
    static const uint8_t source[T_USED] = {GROOVE_808, GROOVE_SAMPLE, GROOVE_DX7, GROOVE_PSG, GROOVE_PLUCK, GROOVE_303};
    if (app.lib)                        /* started before: let go of a chime that is still sounding */
        for (unsigned i = 0; i < 4; ++i)
            if (app.chime[i].note) {
                uint8_t off[3] = {(uint8_t)(0x80 | app.chime_channel), app.chime[i].note, 0};
                (void)fm1_midi_post(off, 3);
            }
    memset(&app, 0, sizeof app);
    app.lib = library;
    app.slot[0] = app.slot[1] = app.last[0] = app.last[1] = NONE;
    app.kit = app.instrument = NONE;
    app.debug_volume = -1;
    app.melody_track = T_FM;
    app.dice = 0x9e3779b9U;
    for (unsigned k = 0; k < AJAPP_KNOBS; ++k) app.knob[k] = 64;
    ajb_fx_init(&app.fx);
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) {
        if (t < T_USED && groove_source_type(t) != source[t]) groove_source_set(t, source[t]);
        groove_sequence_replace(t, 0, empty, STEPS);
        if (groove_sequence_length(t) != STEPS) groove_sequence_length_set(t, STEPS);
        set_mute(t, 0);                 /* the start-up project mutes some tracks */
    }
    set_sends();
    /* What was saved, each checked before use; anything missing or bad falls back to the defaults. */
    app.parent.default_bpm = AJAPP_BPM_DEFAULT;
    ajapp_parent_t parent;
    if (!ajstore_load(AJSTORE_PARENT, &parent, sizeof parent) && parent.default_bpm >= AJAPP_BPM_MIN &&
        parent.default_bpm <= AJAPP_BPM_MAX && parent.swap_on_bar <= 1 && parent.bluetooth <= 1 && parent.fx_speed < AJAPP_SPEEDS) app.parent = parent;
    fx_speed_apply();
    ++app.quiet;
    ajapp_set_bpm(app.parent.default_bpm);
    app.swap_to[AJAPP_DRUMS] = app.swap_to[AJAPP_MELODY] = NONE;
    bluetooth_apply();
    --app.quiet;
    if (!ajstore_load(AJSTORE_HEARTS, &app.hearts, sizeof app.hearts)) {
        unsigned good = app.hearts.count <= AJAPP_HEARTS && app.hearts.newest < AJAPP_HEARTS;
        for (unsigned i = 0; good && i < app.hearts.count; ++i) good = snapshot_valid(&app.hearts.heart[i]);
        if (!good) memset(&app.hearts, 0, sizeof app.hearts);
    } else memset(&app.hearts, 0, sizeof app.hearts);
    if (!ajstore_load(AJSTORE_JAM, &app.jam, sizeof app.jam) && app.jam.count <= AJAPP_JAM_EVENTS &&
        app.jam.length <= AJAPP_JAM_SECONDS * 100U && snapshot_valid(&app.jam.start)) app.has_jam = 1;
    else memset(&app.jam, 0, sizeof app.jam);
    struct snapshot memory;
    if (!ajstore_load(AJSTORE_MEMORY, &memory, sizeof memory) && snapshot_valid(&memory)) {
        app.memory = memory;
        snapshot_apply(&memory, 0);     /* the last pair waits for PLAY/STOP */
    }
    app.dirty = 0;
}
void ajapp_status(ajapp_status_t *st)
{
    unsigned running, step;
    position(&running, &step);
    memset(st, 0, sizeof *st);
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) {
        st->slot[side] = (int8_t)app.slot[side];
        st->view_page[side] = app.view_page[side];
        st->stopping[side] = app.stopping[side];
        st->muted[side] = app.muted[side];
    }
    st->running = (uint8_t)running; st->step = (uint8_t)step;
    st->finishing = app.finishing; st->fill = app.fill; st->fast = app.fast;
    st->effects = app.effects; st->mood = app.mood;
    memcpy(st->knob, app.knob, sizeof st->knob);
    st->kit = (uint8_t)(app.kit == NONE ? 255 : app.kit);
    st->instrument = (uint8_t)(app.instrument == NONE ? 255 : app.instrument);
    st->octave = app.octave; st->bpm = (uint16_t)app.bpm;
    st->recording = app.recording; st->replaying = app.replaying;
    st->hearts = app.hearts.count; st->heart = app.heart_at; st->has_jam = app.has_jam;
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) st->waiting[side] = (int8_t)(app.swap_to[side] == NONE ? -1 : app.swap_to[side]);
    st->parent_open = app.parent_open; st->parent_row = app.parent_row; st->parent_result = app.parent_result;
}
void ajapp_debug_volume(int volume) { app.debug_volume = volume; }
