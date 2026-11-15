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
} app;

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
    for (unsigned k = 0; k < in->knob_count; ++k) tone_set(track, 0, in->knobs[k].parameter, in->knobs[k].value);
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
        groove_sequence_replace(app.melody_track, 0, empty, STEPS);
        return;
    }
    const ajb_melody_loop_t *m = melody_loop(app.slot[AJAPP_MELODY]);
    unsigned count = melody_cells(m, app.mood, app.fast ? 2 : 1, cells);
    if (groove_sequence_length(app.melody_track) != count) groove_sequence_length_set(app.melody_track, count);
    write_from(app.melody_track, cells, count, from % count, keep);
}
static void set_melody_sound(void)
{
    if (app.slot[AJAPP_MELODY] == NONE) return;
    const ajb_instrument_t *in = instrument_of(melody_loop(app.slot[AJAPP_MELODY]));
    unsigned track = track_of(in);
    if (track != app.melody_track) {
        groove_sequence_replace(app.melody_track, 0, empty, STEPS);
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
    app.stopping[side] = 0;
    if (side == AJAPP_DRUMS) write_drums(); else write_melody(0, 0);
    if (app.slot[AJAPP_DRUMS] == NONE && app.slot[AJAPP_MELODY] == NONE) transport(0xfc);
}

/* ---- controls ---- */

void ajapp_pick(unsigned side, unsigned index)
{
    if (!app.lib || side >= AJAPP_SIDES || index >= app.lib->page_count * AJB_SLOTS) return;
    unsigned running, step;
    position(&running, &step);
    if (running && app.slot[side] == (int)index && !app.stopping[side]) {
        app.stopping[side] = 1;         /* the playing key again: stop on the next beat */
        app.stop_step[side] = (uint8_t)((step / 4U + 1U) * 4U % STEPS);
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
}
void ajapp_dice(unsigned side)
{
    if (!app.lib || side >= AJAPP_SIDES) return;
    unsigned total = app.lib->page_count * AJB_SLOTS, index;
    do { app.dice = app.dice * 1664525U + 1013904223U; index = (app.dice >> 8) % total; } while ((int)index == app.slot[side]);
    app.view_page[side] = (uint8_t)(index / AJB_SLOTS);
    if (app.stopping[side]) app.stopping[side] = 0;
    start_side(side, (int)index);
}
void ajapp_hold(unsigned hold, int down)
{
    unsigned on = down != 0;
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
        write_from(app.melody_track, cells, count, bar % count, keep < count ? keep : 0);
    }
    app.finishing = 1;
    app.finish_step = (uint8_t)bar;
    app.stopping[AJAPP_DRUMS] = app.stopping[AJAPP_MELODY] = 0;
}
void ajapp_play_stop(void)
{
    unsigned running, step;
    position(&running, &step);
    if (running) {
        app.finishing = 0;
        app.slot[AJAPP_DRUMS] = app.slot[AJAPP_MELODY] = NONE;
        app.stopping[AJAPP_DRUMS] = app.stopping[AJAPP_MELODY] = 0;
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
    unsigned running, step;
    position(&running, &step);
    app.effects = (uint8_t)(on ? app.effects | (1U << effect) : app.effects & ~(1U << effect));
    if (effect == AJAPP_FX_ECHO || effect == AJAPP_FX_SPARKLE) { if (on) app.fx.last = (uint8_t)effect; set_sends(); }
    else ajb_fx_switch(&app.fx, effect, on, app.bpm, (float)step / 4.0f);
}
void ajapp_home(void)
{
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) ajapp_effect(k, 0);
    app.knob[AJAPP_KNOB_SWOOSH] = 64;
    ajb_fx_swoosh(&app.fx, 64);
}
void ajapp_knob(unsigned knob, unsigned value)
{
    if (knob >= AJAPP_KNOBS) return;
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
}
void ajapp_set_kit(unsigned kit)
{
    if (!app.lib || kit >= app.lib->kit_count) return;
    app.kit = (int)kit;
    set_drum_sound();
}
void ajapp_set_instrument(unsigned instrument)
{
    if (!app.lib || instrument >= app.lib->instrument_count) return;
    app.instrument = (int)instrument;
    set_melody_sound();
    write_melody(0, 0);
}
void ajapp_set_octave(int octave)
{
    app.octave = (int8_t)(octave < -2 ? -2 : octave > 2 ? 2 : octave);
    write_melody(0, 0);
}
/* Mood lands on the next bar line while playing: the rest of this bar keeps its notes. */
void ajapp_set_mood(unsigned mood)
{
    if (mood >= AJAPP_MOODS) return;
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

void ajapp_tick(void)
{
    if (!app.lib) return;
    unsigned running, step;
    position(&running, &step);
    for (unsigned side = 0; side < AJAPP_SIDES; ++side)
        if (app.stopping[side] && (!running || reached(step, app.stop_step[side]))) clear_side(side);
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
void ajapp_audio(int32_t *block, unsigned frames)
{
    static int32_t left[AJB_BLOCK], right[AJB_BLOCK], mono[AJB_BLOCK];
    if (frames != AJB_BLOCK) return;
    unsigned running, step;
    position(&running, &step);
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
    ajapp_set_bpm(AJAPP_BPM_DEFAULT);
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
    st->kit = (uint8_t)(app.kit == NONE ? 255 : app.kit);
    st->instrument = (uint8_t)(app.instrument == NONE ? 255 : app.instrument);
    st->octave = app.octave; st->bpm = (uint16_t)app.bpm;
}
void ajapp_debug_volume(int volume) { app.debug_volume = volume; }
