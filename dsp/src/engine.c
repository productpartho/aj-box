/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box engine. Drums use FM1 Open's DRUM 808 circuit (third_party/synth/
 * drum808.c); melodies use its DX7 core (web/synth/dx7*.c), Pluck string
 * (third_party/synth/pluck.c), PSG chip (web/synth/psg.c) and 303 bass
 * (third_party/synth/bass303.c), all compiled unchanged from firmware/, plus
 * one sliced and one pitched SEA5 sample voice. Effects are in effects.c.
 * The output goes through FM1 Open's master limiter (web/synth/mixer.c).
 *
 * Timing: the engine works in 64-frame ticks (the DX7 control quantum).
 * A clock step (or half step, for Fast) that starts inside a tick plays at
 * the start of that tick, at most 63 samples (1.4 ms) early; step times
 * themselves never drift. Controls apply at the next tick. */
#include "ajbox.h"
#include "clock.h"
#include "sampler.h"
#include "effects.h"
#include "drum808_state.h"
#include "voices.h"
#include "bass303_state.h"
#include "synth_dx7.h"
#include "mixer.h"
#include "fixed.h"
#include "fm_patch.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(AJB_BD == D8_BD && AJB_CH == D8_CH && AJB_PARTS == D8_NUM,
               "AJ Box part order must match DRUM 808");
_Static_assert(AJB_BLOCK == FM1_BLOCK_FRAMES && AJB_RATE == FM1_SAMPLE_RATE,
               "AJ Box runs at the FM1 block format");
_Static_assert(AJB_BLOCK % AJB_TICK == 0 && AJB_TICK == SYNTH_DX7_BLOCK_SAMPLES,
               "tick is the DX7 quantum");
_Static_assert(sizeof(ajb_snapshot_t) == 32, "PRD: a favourite is 32 bytes");
_Static_assert(AJB_CTLS <= 16 && AJB_JAM_TICKS < (1 << 17), "jam events pack into 32 bits");

enum { DX7_VOICES = 8, HELD_MAX = 8, NONE = -1, CYCLE = 64 };   /* CYCLE: 4 bars of 16ths */
enum { FINISH_RING = 32 };      /* steps the Finish chord and crash ring before the clock stops */
enum { FINISH_TRIM = 16384 };   /* Q15, -6 dB: three or four notes at once land near one loop's level */

/* Mood scales (PRD "Buttons and knobs", Mood). Dreamy moves the root up a
 * fourth to F4 rather than down a fifth, so low riffs stay clear. */
static const uint8_t mood_scale[AJB_MOODS][5] = {
    {0,2,4,7,9},    /* Happy: C major pentatonic */
    {0,2,4,7,9},    /* Dreamy: F major pentatonic */
    {0,3,5,7,10},   /* Spooky: C minor pentatonic */
    {0,2,4,6,8},    /* Magic: whole tone */
    {0,2,5,7,10},   /* Floaty: suspended */
    {0,2,3,7,8},    /* Faraway: C D Eb G Ab */
    {0,1,4,7,8},    /* Adventure: C Db E G Ab */
};
static const uint8_t mood_root[AJB_MOODS] = {60, 65, 60, 60, 60, 60, 60};

typedef struct {
    int page, slot;             /* playing loop, slot NONE when silent */
    int pending_page, pending;  /* loop waiting to join, NONE when none */
    uint8_t stopping;           /* stop requested for the next beat */
    uint8_t join_now;           /* pending press landed inside the grace window */
    int fade;                   /* samples left in the stop fade, 0 = none */
    uint64_t press_sample;
    uint8_t mute_held, muted;   /* Mute key; muted until the beat after release */
    int mute_level;             /* 0..AJB_FADE_SAMPLES */
} side_t;

typedef struct { uint8_t note, kind; uint64_t off_half; } held_t;

enum { MELODY_ENGINES = AJB_INSTRUMENT_BASS + 1 };
enum { SNAP_HEART, SNAP_JAM, SNAP_RESTORE };

struct ajb_engine {
    /* Pluck, PSG and 303 state wants 32-byte alignment (pluck.h, psg.h). */
    _Alignas(32) pluck_t pluck;
    _Alignas(32) psg_t psg;
    _Alignas(32) bass303_t bass;
    void *allocation;
    uint8_t bass_note;          /* sounding 303 note, 0 = gate closed */
    uint16_t engine_gain[MELODY_ENGINES];   /* last instrument gain per engine,
                                             * so tails keep their level */
    const ajb_library_t *library;
    ajb_clock_t clock;
    side_t side[AJB_SIDES];
    unsigned view_page[AJB_SIDES];
    uint64_t now;
    uint64_t half_step;         /* step whose midpoint is next (Fast) */
    unsigned mood, next_mood;   /* next_mood waits for the bar line */
    int octave;
    unsigned swap_mode;
    uint32_t trim[AJB_SIDES];
    uint8_t knob[AJB_KNOBS];
    uint8_t fill, fast;

    drum808_t drums;
    int kit_applied, kit_choice;          /* kit index on the 808; PRESETS choice or NONE */
    float drum_gain;
    uint16_t chord_trim;                  /* Q15 on the synth melody engines: the Finish chord's
                                           * notes add up, so it plays below unity */
    ajb_voice_t slice_voice;

    synth_dx7_t *fm;
    uint8_t patch[156], chime_patch[156];
    const ajb_instrument_t *instrument;   /* sounding now */
    int instrument_choice;                /* ALGORITHM choice or NONE */
    ajb_voice_t note_voice;
    held_t held[HELD_MAX];

    uint8_t finishing;          /* waiting for the bar, then ringing out */
    uint64_t finish_end;
    int last_page[AJB_SIDES], last_slot[AJB_SIDES];   /* PLAY/STOP resume */
    uint32_t dice_count;
    struct { uint64_t at; uint8_t note, velocity; } chime[8];
    unsigned chime_count;

    ajb_hearts_t hearts;
    ajb_jam_t jam;
    uint8_t recording, replaying, replay_source;
    uint64_t jam_tick0;
    unsigned replay_index;

    ajb_fx_t fx;
    ajb_event_fn hook;
    void *hook_context;
    uint32_t peak[AJB_SIDES];
    int32_t bus[AJB_SIDES][AJB_BLOCK], mix[AJB_BLOCK], left[AJB_BLOCK], right[AJB_BLOCK];
};

/* ---- library ---- */

static const ajb_drum_loop_t *drum_at(const ajb_engine_t *e, int page, int slot)
{
    return slot < 0 || page < 0 || (unsigned)page >= e->library->page_count ? NULL
         : e->library->pages[page].drums[slot];
}
static const ajb_melody_loop_t *melody_at(const ajb_engine_t *e, int page, int slot)
{
    return slot < 0 || page < 0 || (unsigned)page >= e->library->page_count ? NULL
         : e->library->pages[page].melody[slot];
}
static int loop_exists(const ajb_engine_t *e, unsigned s, int page, int slot)
{
    return s == AJB_DRUMS ? drum_at(e, page, slot) != NULL : melody_at(e, page, slot) != NULL;
}
static unsigned loop_steps(const ajb_engine_t *e, unsigned s, int page, int slot)
{
    return s == AJB_DRUMS ? drum_at(e, page, slot)->steps : melody_at(e, page, slot)->steps;
}
static void emit(ajb_engine_t *e, unsigned kind, unsigned side, unsigned page, unsigned slot)
{
    if (!e->hook) return;
    ajb_event_t ev = {e->now, e->now, e->clock.next_step, side, slot, 0, kind, page};
    e->hook(e->hook_context, &ev);
}

/* ---- drum kits ---- */

/* Every 808 parameter to the kit's value or the default, in place, so a
 * PRESETS turn never cuts a ringing cymbal. */
static void apply_kit(ajb_engine_t *e, int index)
{
    const ajb_kit_t *kit = index >= 0 && (unsigned)index < e->library->kit_count ? e->library->kits[index] : NULL;
    for (int part = 0; part <= D8_KIT; ++part)
        for (int p = 0; p < drum808_nparams(part); ++p) {
            int value = drum808_param(part, p)->def;
            for (unsigned i = 0; kit && i < kit->count; ++i)
                if (kit->edits[i].part == part && kit->edits[i].parameter == p) value = kit->edits[i].value;
            if (drum808_get(&e->drums, part, p) != value) drum808_set(&e->drums, part, p, value);
        }
    e->kit_applied = index;
}
static int wanted_kit(const ajb_engine_t *e)
{
    if (e->kit_choice != NONE) return e->kit_choice;
    const side_t *d = &e->side[AJB_DRUMS];
    int page = d->slot != NONE ? d->page : d->pending != NONE ? d->pending_page : (int)e->view_page[AJB_DRUMS];
    return e->library->kit_count ? e->library->pages[page].kit : NONE;
}

/* ---- melody engines ---- */

/* Knobs the instrument doesn't list go back to the engine defaults. */
static unsigned knob_value(const ajb_instrument_t *in, unsigned parameter, unsigned fallback)
{
    for (unsigned i = 0; in && i < in->knob_count && i < AJB_KNOBS_MAX; ++i)
        if (in->knobs[i].parameter == parameter) return in->knobs[i].value;
    return fallback;
}
static void apply_knobs(ajb_engine_t *e, const ajb_instrument_t *in)
{
    switch (in ? in->kind : AJB_INSTRUMENT_FM) {
    case AJB_INSTRUMENT_PLUCK:
        for (unsigned k = 0; k < PLUCK_PARAMS; ++k) pluck_set(&e->pluck, k, knob_value(in, k, pluck_param(k)->def));
        break;
    case AJB_INSTRUMENT_PSG:
        for (unsigned k = 0; k < PSG_PARAMS; ++k) psg_set(&e->psg, k, knob_value(in, k, psg_param(k)->def));
        break;
    case AJB_INSTRUMENT_BASS:
        for (int k = 0; k < BASS303_NPARAMS; ++k)
            bass303_set(&e->bass, k, (int)knob_value(in, (unsigned)k, bass303_param(k)->def));
        break;
    default: break;
    }
}
static void reset_melody_engines(ajb_engine_t *e)
{
    pluck_init(&e->pluck);
    psg_init(&e->psg);
    bass303_init(&e->bass);
    e->bass_note = 0;
    apply_knobs(e, e->instrument);
}

/* Key on (velocity > 0) or off for one melody engine. The 303 is mono:
 * a new note retriggers it, and only its sounding note's key-off closes
 * the gate. Velocity 100 and up is a 303 accent. The PSG chip's 16 volume
 * steps drop about 3 dB for every 8 of velocity, far steeper than the other
 * engines, so its velocity is lifted: 100 and up is full volume, and a loop
 * written for another instrument stays audible on a chip sound. */
static void engine_key(ajb_engine_t *e, unsigned kind, unsigned note, unsigned velocity, uint8_t *patch)
{
    switch (kind) {
    case AJB_INSTRUMENT_PLUCK: pluck_note(&e->pluck, note, velocity, 0); break;
    case AJB_INSTRUMENT_PSG:
        psg_note(&e->psg, note, velocity ? (velocity + 27 > 127 ? 127 : velocity + 27) : 0, 0);
        break;
    case AJB_INSTRUMENT_BASS:
        if (velocity) {
            bass303_note_on(&e->bass, (int)note, velocity >= 100, 0, 0);
            e->bass_note = (uint8_t)note;
        } else if (e->bass_note == note) {
            bass303_note_off(&e->bass);
            e->bass_note = 0;
        }
        break;
    default: {
        uint8_t message[3] = {(uint8_t)(velocity ? 0x90 : 0x80), (uint8_t)note, (uint8_t)velocity};
        (void)synth_dx7_midi(e->fm, patch, message, 3);
        break;
    }
    }
}
static void release_held(ajb_engine_t *e, uint64_t half, int all)
{
    for (unsigned i = 0; i < HELD_MAX; ++i) {
        if (!e->held[i].note || (!all && e->held[i].off_half > half)) continue;
        engine_key(e, e->held[i].kind, e->held[i].note, 0, e->patch);
        e->held[i].note = 0;
    }
}
static void select_instrument(ajb_engine_t *e, const ajb_instrument_t *in)
{
    if (e->instrument == in) return;
    release_held(e, 0, 1);
    e->instrument = in;
    if (in->kind == AJB_INSTRUMENT_FM) groove_fm_factory(e->patch, in->program);
    if (in->kind < MELODY_ENGINES) e->engine_gain[in->kind] = in->gain_q15;
    apply_knobs(e, in);
}
static const ajb_instrument_t *wanted_instrument(const ajb_engine_t *e, const ajb_melody_loop_t *m)
{
    if (e->instrument_choice != NONE && (unsigned)e->instrument_choice < e->library->instrument_count)
        return e->library->instruments[e->instrument_choice];
    return m->instrument;
}

/* ---- melody notes ---- */

static int degree_note(const ajb_engine_t *e, const ajb_melody_loop_t *m, int degree)
{
    return mood_root[e->mood] + 12 * (m->octave + e->octave) + mood_scale[e->mood][degree % 5] + 12 * (degree / 5);
}
/* A note that lasts `halves` half steps from half step `half`. PRESETS and
 * ALGORITHM swaps land here, on the next note (PRD "Playback rules" 9). */
static void play_note(ajb_engine_t *e, const ajb_melody_loop_t *m, int note, unsigned velocity, uint64_t half,
                      unsigned halves)
{
    const ajb_instrument_t *in = wanted_instrument(e, m);
    select_instrument(e, in);
    if (note < 1 || note > 127) return;
    if (in->kind == AJB_INSTRUMENT_SAMPLE) {
        ajb_voice_note(&e->note_voice, in->sample, note, (int32_t)in->gain_q15 * (int32_t)velocity / 127);
        return;
    }
    for (unsigned i = 0; i < HELD_MAX; ++i)
        if (e->held[i].note == note) { engine_key(e, e->held[i].kind, (unsigned)note, 0, e->patch); e->held[i].note = 0; }
    unsigned slot = 0;
    for (unsigned i = 0; i < HELD_MAX; ++i)
        if (!e->held[i].note) { slot = i; break; }
    if (e->held[slot].note) engine_key(e, e->held[slot].kind, e->held[slot].note, 0, e->patch);
    e->held[slot].note = (uint8_t)note;
    e->held[slot].kind = in->kind;
    e->held[slot].off_half = half + halves;
    engine_key(e, in->kind, (unsigned)note, velocity, e->patch);
}

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static int degree_of(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static unsigned chord_degrees(const char *text, int chord[AJB_CHORD_MAX])
{
    unsigned count = 0;
    for (; text && count < AJB_CHORD_MAX && degree_of(text[count]) >= 0; ++count) chord[count] = degree_of(text[count]);
    return count;
}
/* Loop position `pos` starting at half step `half`; `per_step` half steps
 * per loop step (2 normally, 1 while Fast is held). */
static void melody_step(ajb_engine_t *e, const ajb_melody_loop_t *m, unsigned pos, uint64_t half, unsigned per_step)
{
    e->chord_trim = 32768;   /* a loop is playing again: back to full level after a Finish */
    if (m->riff) {
        int degree = degree_of(m->riff[pos]);
        if (degree < 0) return;
        unsigned length = 1;
        while (pos + length < m->steps && m->riff[pos + length] == '-') ++length;
        play_note(e, m, degree_note(e, m, degree), m->velocity, half, length * per_step);
        return;
    }
    if (!m->rate || pos % m->rate) return;
    const char *chord_text = m->chord[(pos / 16U) % AJB_ARP_BARS];
    if (!chord_text) chord_text = m->chord[0];
    int chord[AJB_CHORD_MAX];
    unsigned count = chord_degrees(chord_text, chord);
    unsigned tones = count * (m->octaves ? m->octaves : 1U);
    if (!tones) return;
    unsigned n = pos / m->rate, k;
    switch (m->dir) {
    case AJB_ARP_DOWN: k = tones - 1U - n % tones; break;
    case AJB_ARP_BOUNCE: {
        unsigned period = tones > 1 ? 2U * tones - 2U : 1U, i = n % period;
        k = i < tones ? i : period - i;
        break;
    }
    case AJB_ARP_RANDOM: k = hash32(n * 2654435761U + m->steps) % tones; break;
    default: k = n % tones; break;
    }
    int degree = chord[k % count] + 5 * (int)(k / count);
    play_note(e, m, degree_note(e, m, degree), m->velocity, half, m->rate * per_step);
}

/* ---- drums ---- */

static void drum_hits(ajb_engine_t *e, const ajb_drum_loop_t *d, unsigned pos)
{
    for (unsigned p = 0; p < AJB_PARTS; ++p) {
        if (!d->parts[p]) continue;
        char c = d->parts[p][pos];
        float velocity = c == 'X' ? 1.0f : c == 'x' ? 0.78f : c == 'o' ? 0.45f : 0.0f;
        if (velocity > 0) drum808_trigger(&e->drums, (int)p, velocity);
    }
}
static void drum_step(ajb_engine_t *e, const ajb_drum_loop_t *d, uint64_t global)
{
    int kit = wanted_kit(e);
    if (kit != e->kit_applied) apply_kit(e, kit);          /* PRESETS: on the next note */
    const ajb_drum_loop_t *fill = e->library->fill;
    if (e->fill && fill) {                                  /* Fill replaces the loop while held */
        ajb_voice_stop(&e->slice_voice);
        drum_hits(e, fill, (unsigned)(global % fill->steps));
        return;
    }
    unsigned pos = (unsigned)(global % d->steps);
    if (d->sliced) ajb_voice_slice(&e->slice_voice, d->sliced, pos, d->gain_q15);
    else drum_hits(e, d, pos);
}

/* ---- sides ---- */

static void play_step(ajb_engine_t *e, unsigned s, uint64_t global, uint64_t exact)
{
    side_t *side = &e->side[s];
    unsigned steps = loop_steps(e, s, side->page, side->slot);
    unsigned pos = (unsigned)(global % steps);
    if (s == AJB_DRUMS) drum_step(e, drum_at(e, side->page, side->slot), global);
    else if (e->fast) melody_step(e, melody_at(e, side->page, side->slot), (unsigned)((2U * global) % steps), 2U * global, 1);
    else melody_step(e, melody_at(e, side->page, side->slot), pos, 2U * global, 2);
    if (e->hook) {
        ajb_event_t ev = {e->now, exact, global, s, (unsigned)side->slot, pos, AJB_EVENT_STEP, (unsigned)side->page};
        e->hook(e->hook_context, &ev);
    }
}

static void join(ajb_engine_t *e, unsigned s)
{
    side_t *side = &e->side[s];
    side->page = side->pending_page;
    side->slot = side->pending;
    side->pending = NONE;
    side->stopping = 0;
    if (s == AJB_MELODY) release_held(e, 0, 1);
    else if (drum_at(e, side->page, side->slot)->sliced == NULL) ajb_voice_stop(&e->slice_voice);
}

static void silence_side(ajb_engine_t *e, unsigned s)
{
    if (s == AJB_DRUMS) {
        drum808_t fresh;
        drum808_init(&fresh);               /* stop every voice, keep the kit */
        int kit = e->kit_applied;
        e->drums = fresh;
        e->kit_applied = NONE;
        apply_kit(e, kit);
        drum808_pan_settle(&e->drums);
        ajb_voice_reset(&e->slice_voice);
    } else {
        release_held(e, 0, 1);
        fm1_dx7_panic(e->fm);
        reset_melody_engines(e);
        ajb_voice_reset(&e->note_voice);
    }
}

static void start_clock_if_idle(ajb_engine_t *e)
{
    if (e->clock.running) return;
    /* PRD "Playback rules" 2: from silence, start the clock on step 1. */
    ajb_clock_start(&e->clock);
    e->half_step = 0;
    for (unsigned o = 0; o < AJB_SIDES; ++o) e->side[o].fade = 0;
}

/* An exact loop on a side: what a key press, Dice and hearts all become. */
static void pick(ajb_engine_t *e, unsigned s, int page, int slot)
{
    side_t *side = &e->side[s];
    if (!loop_exists(e, s, page, slot)) return;
    e->finishing = 0;
    start_clock_if_idle(e);
    if (side->slot == slot && side->page == page && side->pending == NONE && !side->fade) {
        side->stopping = 1;           /* rule 4: same key again stops it */
        return;
    }
    if (side->slot == slot && side->page == page && side->pending != NONE) {
        side->pending = NONE;         /* changed mind back to the playing loop */
        side->stopping = 0;
        return;
    }
    /* A drum loop from another page brings that page's kit back. */
    if (s == AJB_DRUMS && page != side->page) e->kit_choice = NONE;
    if (s == AJB_MELODY) e->instrument_choice = NONE;
    side->pending_page = page;
    side->pending = slot;             /* rule 3: swap, phase-locked */
    side->stopping = 0;
    side->press_sample = e->now;
    /* Rule 3 joins "from whatever step the beat clock is on": for a silent
     * side pressed inside the grace window that is the current step, played
     * now; otherwise the next one. A playing side swaps on the next step, so
     * the old loop's hits on this step are never doubled. */
    uint64_t age = e->clock.elapsed - e->clock.last_start;
    side->join_now = e->swap_mode == AJB_SWAP_STEP && side->slot == NONE && !side->fade &&
                     e->clock.next_step > 0 && age < AJB_GRACE_SAMPLES &&
                     ajb_clock_until_next(&e->clock) >= AJB_TICK;
}

static void stop_both(ajb_engine_t *e)
{
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        side_t *side = &e->side[s];
        if (side->slot != NONE) side->fade = AJB_FADE_SAMPLES;
        side->slot = side->pending = NONE;
        side->stopping = side->join_now = 0;
    }
    release_held(e, 0, 1);
    e->finishing = 0;
}

/* Finish (rule 7): a crash and the melody's final chord on the bar. */
static void finish_now(ajb_engine_t *e, uint64_t step)
{
    drum808_trigger(&e->drums, AJB_CY, 1.0f);
    drum808_trigger(&e->drums, AJB_BD, 1.0f);
    side_t *m = &e->side[AJB_MELODY];
    if (m->slot != NONE) {
        const ajb_melody_loop_t *loop = melody_at(e, m->page, m->slot);
        int chord[AJB_CHORD_MAX];
        unsigned count = chord_degrees(loop->riff ? "023" : loop->chord[0], chord);
        /* The 303 and the sample voice play one note at a time: no trim for them. */
        unsigned kind = e->instrument ? e->instrument->kind : AJB_INSTRUMENT_SAMPLE;
        if (count > 1 && kind != AJB_INSTRUMENT_BASS && kind != AJB_INSTRUMENT_SAMPLE) e->chord_trim = FINISH_TRIM;
        for (unsigned i = 0; i < count && i < 4; ++i)
            play_note(e, loop, degree_note(e, loop, chord[i]), 110, 2U * step, 2U * 16U);
    }
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        e->side[s].slot = e->side[s].pending = NONE;
        e->side[s].stopping = 0;
    }
    e->finish_end = step + FINISH_RING;
    emit(e, AJB_EVENT_FINISH, 0, 0, 0);
}

/* ---- the clock ---- */

static void fire_step(ajb_engine_t *e)
{
    uint64_t exact = e->now + ajb_clock_until_next(&e->clock);
    uint64_t step = ajb_clock_fire(&e->clock);
    e->half_step = step;
    release_held(e, 2U * step, 0);
    if (step % 16U == 0) e->mood = e->next_mood;            /* Mood changes on the bar */
    if (e->finishing == 1 && step % 16U == 0) {
        finish_now(e, step);
        e->finishing = 2;
        return;
    }
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        side_t *side = &e->side[s];
        if (side->stopping && step % 4U == 0) {
            /* PRD rule 4: stop on the next beat with a 10 ms fade. */
            side->stopping = 0;
            side->slot = NONE;
            side->fade = AJB_FADE_SAMPLES;
            if (s == AJB_MELODY) release_held(e, 0, 1);
        }
        if (side->muted && !side->mute_held && step % 4U == 0) side->muted = 0;   /* back on the beat */
        if (side->pending != NONE && (e->swap_mode == AJB_SWAP_STEP || step % 16U == 0 ||
                                      (side->slot == NONE && step == 0))) {
            side->join_now = 0;
            join(e, s);
        }
        if (side->slot != NONE) play_step(e, s, step, exact);
    }
}

/* Fast: the melody's in-between notes, on each step's midpoint. */
static void fire_half(ajb_engine_t *e)
{
    uint64_t step = e->half_step++;
    release_held(e, 2U * step + 1U, 0);
    side_t *side = &e->side[AJB_MELODY];
    if (!e->fast || side->slot == NONE) return;
    const ajb_melody_loop_t *m = melody_at(e, side->page, side->slot);
    unsigned pos = (unsigned)((2U * step + 1U) % m->steps);
    melody_step(e, m, pos, 2U * step + 1U, 1);
    if (e->hook) {
        ajb_event_t ev = {e->now, e->now, step, AJB_MELODY, (unsigned)side->slot, pos, AJB_EVENT_STEP,
                          (unsigned)side->page};
        e->hook(e->hook_context, &ev);
    }
}
static uint64_t until_half(const ajb_engine_t *e)
{
    uint64_t target = e->half_step * AJB_STEP_PHASE + AJB_STEP_PHASE / 2U;
    if (e->half_step >= e->clock.next_step) return UINT64_MAX;      /* that step hasn't started */
    return e->clock.phase >= target ? 0 : (target - e->clock.phase + e->clock.bpm - 1U) / e->clock.bpm;
}

static void tick_events(ajb_engine_t *e)
{
    if (!e->clock.running) return;
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        side_t *side = &e->side[s];
        if (side->pending != NONE && side->join_now) {
            side->join_now = 0;
            join(e, s);
            play_step(e, s, e->clock.next_step - 1U, side->press_sample);
        }
    }
    for (;;) {
        uint64_t step = ajb_clock_until_next(&e->clock), half = until_half(e);
        if (step >= AJB_TICK && half >= AJB_TICK) break;
        if (half < step) fire_half(e);
        else fire_step(e);
    }
}

/* ---- audio ---- */

static void render_tick(ajb_engine_t *e, unsigned offset)
{
    int32_t *drums = e->bus[AJB_DRUMS] + offset, *melody = e->bus[AJB_MELODY] + offset;
    memset(drums, 0, AJB_TICK * sizeof *drums);
    memset(melody, 0, AJB_TICK * sizeof *melody);

    /* DRUM 808 in 32-frame chunks, float to Q24 as in web/synth/voices.c. */
    const ajb_drum_loop_t *loop = drum_at(e, e->side[AJB_DRUMS].page, e->side[AJB_DRUMS].slot);
    const ajb_drum_loop_t *level = e->fill && e->library->fill ? e->library->fill : loop;   /* Fill has its own gain */
    float loop_gain = level && !level->sliced ? level->gain_q15 / 32768.0f : e->drum_gain;
    if (level && !level->sliced) e->drum_gain = loop_gain;   /* tails after a stop keep their level */
    if (e->drum_gain == 0.0f) e->drum_gain = loop_gain = 0.5f;
    for (unsigned chunk = 0; chunk < AJB_TICK; chunk += 32) {
        float dry[32] = {0}, send[32] = {0};
        drum808_render(&e->drums, dry, send, send, 32);
        for (unsigned i = 0; i < 32; ++i) {
            float v = dry[i] * loop_gain;
            int32_t q = !(v == v) ? 0 : v >= 128.0f ? INT32_MAX : v <= -128.0f ? INT32_MIN
                      : (int32_t)(v * 16777216.0f);
            drums[chunk + i] = fm_add_sat(drums[chunk + i], q);
        }
    }
    ajb_voice_render(&e->slice_voice, drums, AJB_TICK);

    /* Every melody engine renders each tick, so the previous instrument's
     * tail rings on after a swap at its own gain. */
    int32_t voice[AJB_TICK];
    for (unsigned kind = 0; kind < MELODY_ENGINES; ++kind) {
        switch (kind) {
        case AJB_INSTRUMENT_FM:
            memset(voice, 0, sizeof voice);
            synth_dx7_render(e->fm, voice, AJB_TICK);
            break;
        case AJB_INSTRUMENT_PLUCK:
            if (!pluck_active(&e->pluck)) continue;
            pluck_render(&e->pluck, voice, AJB_TICK);
            break;
        case AJB_INSTRUMENT_PSG: psg_render(&e->psg, voice, AJB_TICK); break;
        case AJB_INSTRUMENT_BASS: bass303_render(&e->bass, voice, AJB_TICK); break;
        default: continue;
        }
        int32_t gain = (int32_t)((uint32_t)e->engine_gain[kind] * e->chord_trim >> 15);
        for (unsigned i = 0; i < AJB_TICK; ++i)
            melody[i] = fm_add_sat(melody[i], (int32_t)(((int64_t)voice[i] * gain) >> 15));
    }
    ajb_voice_render(&e->note_voice, melody, AJB_TICK);

    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        side_t *side = &e->side[s];
        int32_t *bus = e->bus[s] + offset;
        if (side->fade > 0) {
            for (unsigned i = 0; i < AJB_TICK; ++i) {
                int g = side->fade > 0 ? side->fade-- : 0;
                bus[i] = (int32_t)((int64_t)bus[i] * g / AJB_FADE_SAMPLES);
            }
            if (side->fade == 0) silence_side(e, s);
        }
        /* Mute: a 10 ms ramp down while held, up again on the beat. */
        int target = side->muted ? 0 : AJB_FADE_SAMPLES;
        if (side->mute_level != target || target == 0)
            for (unsigned i = 0; i < AJB_TICK; ++i) {
                if (side->mute_level < target) ++side->mute_level;
                else if (side->mute_level > target) --side->mute_level;
                bus[i] = (int32_t)((int64_t)bus[i] * side->mute_level / AJB_FADE_SAMPLES);
            }
        for (unsigned i = 0; i < AJB_TICK; ++i) {
            uint32_t a = bus[i] < 0 ? 0U - (uint32_t)bus[i] : (uint32_t)bus[i];
            if (a > e->peak[s]) e->peak[s] = a;
        }
    }
}

static void chime_events(ajb_engine_t *e)
{
    unsigned kept = 0;
    for (unsigned i = 0; i < e->chime_count; ++i) {
        if (e->chime[i].at > e->now) { e->chime[kept++] = e->chime[i]; continue; }
        engine_key(e, AJB_INSTRUMENT_FM, e->chime[i].note, e->chime[i].velocity, e->chime_patch);
    }
    e->chime_count = kept;
}

static void apply_control(ajb_engine_t *e, ajb_control_t c, int live);

static void jam_events(ajb_engine_t *e)
{
    if (!e->replaying) return;
    uint64_t tick = e->now / AJB_TICK - e->jam_tick0;
    while (e->replay_index < e->jam.count && (e->jam.event[e->replay_index] >> 15) <= tick) {
        uint32_t v = e->jam.event[e->replay_index++];
        ajb_control_t c = {(uint8_t)((v >> 11) & 15U), (uint8_t)((v >> 8) & 7U), (uint8_t)v};
        apply_control(e, c, 0);
    }
    if (tick >= e->jam.length) {
        e->replaying = 0;
        emit(e, AJB_EVENT_JAM_END, 0, 0, 0);
    }
}

static float beat_position(const ajb_engine_t *e)
{
    if (!e->clock.running) return 0.0f;
    return (float)((double)e->clock.phase / (4.0 * (double)AJB_STEP_PHASE));
}

void ajb_render(ajb_engine_t *e, int32_t out[AJB_BLOCK * 2])
{
    float beats = beat_position(e);
    for (unsigned offset = 0; offset < AJB_BLOCK; offset += AJB_TICK) {
        jam_events(e);
        chime_events(e);
        tick_events(e);
        render_tick(e, offset);
        if (e->clock.running) {
            ajb_clock_advance(&e->clock, AJB_TICK);
            if (e->finishing == 2 && e->clock.next_step >= e->finish_end) {
                e->finishing = 0;
                release_held(e, 0, 1);
            }
            if (!ajb_side_active(e, AJB_DRUMS) && !ajb_side_active(e, AJB_MELODY) && !e->finishing)
                e->clock.running = 0;
        }
        e->now += AJB_TICK;
    }
    /* KNOB 4 balance: the middle plays both at full level. */
    unsigned b = e->knob[AJB_KNOB_BALANCE];
    uint32_t balance[AJB_SIDES] = {b <= 64 ? 32768U : (127U - b) * 32768U / 63U,
                                   b >= 64 ? 32768U : b * 32768U / 64U};
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        int64_t gain = ((int64_t)e->trim[s] * balance[s] >> 15) << 14;   /* Q15 to Q29 */
        fm1_mix128(e->mix, e->bus[s], gain > INT32_MAX ? INT32_MAX : (int32_t)gain, s);
    }
    ajb_fx_process(&e->fx, e->mix, e->left, e->right, e->clock.bpm, beats);
    fm1_bus_output128(out, e->left, e->right);
    fm1_master_process128(out);   /* CLAUDE.md: output limiter always on */
}

/* ---- controls ---- */

static void apply_snapshot(ajb_engine_t *e, const ajb_snapshot_t *snap, unsigned mode);

static void record(ajb_engine_t *e, ajb_control_t c)
{
    if (!e->recording) return;
    uint64_t tick = e->now / AJB_TICK - e->jam_tick0;
    if (tick >= AJB_JAM_TICKS || e->jam.count >= AJB_JAM_EVENTS) {
        e->recording = 0;                  /* 2 minutes, then it keeps what it has */
        e->jam.length = (uint32_t)tick;
        return;
    }
    e->jam.event[e->jam.count++] = (uint32_t)tick << 15 | (uint32_t)(c.type & 15U) << 11 |
                                   (uint32_t)(c.a & 7U) << 8 | c.b;
}

static void apply_control(ajb_engine_t *e, ajb_control_t c, int live)
{
    unsigned a = c.a, b = c.b;
    if (live) {
        /* Taking over: a loop key, PLAY/STOP or HOME ends a replay. */
        if (e->replaying && (c.type == AJB_CTL_PRESS || c.type == AJB_CTL_PICK || c.type == AJB_CTL_PLAY_STOP ||
                             c.type == AJB_CTL_HOME))
            e->replaying = 0;
        record(e, c);
    }
    switch (c.type) {
    case AJB_CTL_PRESS:
        if (a < AJB_SIDES && b < AJB_SLOTS) pick(e, a, (int)e->view_page[a], (int)b);
        break;
    case AJB_CTL_PICK:
        if (a < AJB_SIDES) pick(e, a, (int)(b / AJB_SLOTS), (int)(b % AJB_SLOTS));
        break;
    case AJB_CTL_PAGE:
        if (a < AJB_SIDES && b < e->library->page_count) e->view_page[a] = b;
        break;
    case AJB_CTL_HOLD:
        switch (a) {
        case AJB_HOLD_FILL: e->fill = b != 0; break;
        case AJB_HOLD_FAST: e->fast = b != 0; break;
        case AJB_HOLD_MUTE_DRUMS: case AJB_HOLD_MUTE_MELODY: {
            side_t *side = &e->side[a == AJB_HOLD_MUTE_DRUMS ? AJB_DRUMS : AJB_MELODY];
            side->mute_held = b != 0;
            if (b) side->muted = 1;
            else if (!e->clock.running) side->muted = 0;
            break;
        }
        case AJB_HOLD_STUTTER:
            ajb_fx_switch(&e->fx, AJB_FX_STUTTER, b != 0, e->clock.bpm, beat_position(e));
            break;
        default: break;
        }
        break;
    case AJB_CTL_FINISH:
        if (e->clock.running && !e->finishing &&
            (e->side[AJB_DRUMS].slot != NONE || e->side[AJB_MELODY].slot != NONE)) e->finishing = 1;
        break;
    case AJB_CTL_PLAY_STOP:
        if (ajb_side_active(e, AJB_DRUMS) || ajb_side_active(e, AJB_MELODY)) {
            for (unsigned s = 0; s < AJB_SIDES; ++s) {
                const side_t *side = &e->side[s];
                e->last_page[s] = side->slot != NONE ? side->page : side->pending != NONE ? side->pending_page : NONE;
                e->last_slot[s] = side->slot != NONE ? side->slot : side->pending;
            }
            stop_both(e);
        } else {
            for (unsigned s = 0; s < AJB_SIDES; ++s)
                if (e->last_slot[s] != NONE) pick(e, s, e->last_page[s], e->last_slot[s]);
        }
        break;
    case AJB_CTL_HOME:
        for (unsigned k = 0; k < AJB_EFFECTS; ++k) ajb_fx_switch(&e->fx, k, 0, e->clock.bpm, 0.0f);
        e->knob[AJB_KNOB_SWOOSH] = 64;
        ajb_fx_swoosh(&e->fx, 64);
        break;
    case AJB_CTL_EFFECT:
        if (a < AJB_EFFECTS) ajb_fx_switch(&e->fx, a, b != 0, e->clock.bpm, beat_position(e));
        break;
    case AJB_CTL_KNOB:
        if (a >= AJB_KNOBS) break;
        e->knob[a] = (uint8_t)(b > 127 ? 127 : b);
        if (a == AJB_KNOB_SWOOSH) ajb_fx_swoosh(&e->fx, e->knob[a]);
        if (a == AJB_KNOB_AMOUNT || a == AJB_KNOB_SPEED) ajb_fx_knob(&e->fx, a, e->knob[a]);
        break;
    case AJB_CTL_BPM:
        e->clock.bpm = b < AJB_BPM_MIN ? AJB_BPM_MIN : b > AJB_BPM_MAX ? AJB_BPM_MAX : b;
        break;
    case AJB_CTL_KIT:
        if (b < e->library->kit_count) e->kit_choice = (int)b;
        break;
    case AJB_CTL_INSTRUMENT:
        if (b < e->library->instrument_count) e->instrument_choice = (int)b;
        break;
    case AJB_CTL_OCTAVE:
        e->octave = b > 4 ? 2 : (int)b - 2;
        break;
    case AJB_CTL_MOOD:
        if (b < AJB_MOODS) {
            e->next_mood = b;
            if (!e->clock.running) e->mood = b;
        }
        break;
    case AJB_CTL_HEART:
        if (b < e->hearts.count) {
            e->hearts.current = (uint8_t)b;
            apply_snapshot(e, &e->hearts.heart[b], SNAP_HEART);
        }
        break;
    default: break;
    }
}

void ajb_control(ajb_engine_t *e, ajb_control_t c) { apply_control(e, c, 1); }

static void control(ajb_engine_t *e, uint8_t type, unsigned a, unsigned b)
{
    ajb_control_t c = {type, (uint8_t)a, (uint8_t)b};
    apply_control(e, c, 1);
}

void ajb_press(ajb_engine_t *e, unsigned side, unsigned slot)
{
    if (side < AJB_SIDES && slot < AJB_SLOTS) control(e, AJB_CTL_PRESS, side, slot);
}
void ajb_page_step(ajb_engine_t *e, unsigned side, int delta)
{
    if (side >= AJB_SIDES) return;
    int n = (int)e->library->page_count;
    control(e, AJB_CTL_PAGE, side, (unsigned)((((int)e->view_page[side] + delta) % n + n) % n));
}
void ajb_dice(ajb_engine_t *e, unsigned side)
{
    if (side >= AJB_SIDES) return;
    unsigned total = e->library->page_count * AJB_SLOTS, count = 0;
    const side_t *s = &e->side[side];
    for (unsigned i = 0; i < total; ++i)
        count += loop_exists(e, side, (int)(i / AJB_SLOTS), (int)(i % AJB_SLOTS)) &&
                 !(s->slot == (int)(i % AJB_SLOTS) && s->page == (int)(i / AJB_SLOTS));
    if (!count) return;
    unsigned choice = hash32((uint32_t)e->now ^ (++e->dice_count * 2654435761U)) % count;
    for (unsigned i = 0; i < total; ++i) {
        int page = (int)(i / AJB_SLOTS), slot = (int)(i % AJB_SLOTS);
        if (!loop_exists(e, side, page, slot) || (s->slot == slot && s->page == page)) continue;
        if (choice--) continue;
        control(e, AJB_CTL_PICK, side, i);
        control(e, AJB_CTL_PAGE, side, (unsigned)page);   /* the keys show where it came from */
        emit(e, AJB_EVENT_DICE, side, (unsigned)page, (unsigned)slot);
        return;
    }
}
void ajb_hold(ajb_engine_t *e, unsigned hold, int down)
{
    if (hold < AJB_HOLDS) control(e, AJB_CTL_HOLD, hold, down != 0);
}
void ajb_finish(ajb_engine_t *e) { control(e, AJB_CTL_FINISH, 0, 0); }
void ajb_play_stop(ajb_engine_t *e) { control(e, AJB_CTL_PLAY_STOP, 0, 0); }
void ajb_home(ajb_engine_t *e) { control(e, AJB_CTL_HOME, 0, 0); }
void ajb_effect(ajb_engine_t *e, unsigned effect, int on)
{
    if (effect < AJB_EFFECTS) control(e, AJB_CTL_EFFECT, effect, on != 0);
}
void ajb_knob(ajb_engine_t *e, unsigned knob, unsigned value)
{
    if (knob < AJB_KNOBS) control(e, AJB_CTL_KNOB, knob, value > 127 ? 127 : value);
}
void ajb_set_bpm(ajb_engine_t *e, unsigned bpm)
{
    control(e, AJB_CTL_BPM, 0, bpm < AJB_BPM_MIN ? AJB_BPM_MIN : bpm > AJB_BPM_MAX ? AJB_BPM_MAX : bpm);
}
unsigned ajb_bpm(const ajb_engine_t *e) { return e->clock.bpm; }
void ajb_set_kit(ajb_engine_t *e, unsigned kit) { control(e, AJB_CTL_KIT, 0, kit); }
void ajb_set_instrument(ajb_engine_t *e, unsigned instrument) { control(e, AJB_CTL_INSTRUMENT, 0, instrument); }
void ajb_set_octave(ajb_engine_t *e, int octave)
{
    control(e, AJB_CTL_OCTAVE, 0, (unsigned)((octave < -2 ? -2 : octave > 2 ? 2 : octave) + 2));
}
void ajb_set_mood(ajb_engine_t *e, unsigned mood)
{
    if (mood < AJB_MOODS) control(e, AJB_CTL_MOOD, 0, mood);
}
unsigned ajb_mood(const ajb_engine_t *e) { return e->mood; }
void ajb_set_swap_mode(ajb_engine_t *e, unsigned mode) { e->swap_mode = mode == AJB_SWAP_BAR ? AJB_SWAP_BAR : AJB_SWAP_STEP; }

/* Hello: C5 E5 G5 C6 on Tub Bells, quiet, about a second. */
void ajb_chime(ajb_engine_t *e)
{
    static const uint8_t notes[4] = {72, 76, 79, 84};
    groove_fm_factory(e->chime_patch, 120);
    e->chord_trim = 32768;
    if (!e->instrument || e->instrument->kind != AJB_INSTRUMENT_FM) e->engine_gain[AJB_INSTRUMENT_FM] = 4000;
    e->chime_count = 0;
    for (unsigned i = 0; i < 4; ++i) {
        e->chime[e->chime_count++] = (__typeof__(e->chime[0])){e->now + i * (AJB_RATE / 8U), notes[i], 80};
        e->chime[e->chime_count++] = (__typeof__(e->chime[0])){e->now + AJB_RATE * 7U / 10U + i * 64U, notes[i], 0};
    }
}

/* ---- snapshots, hearts and jams ---- */

void ajb_snapshot(const ajb_engine_t *e, ajb_snapshot_t *snap)
{
    memset(snap, 0, sizeof *snap);
    const side_t *d = &e->side[AJB_DRUMS], *m = &e->side[AJB_MELODY];
    int dp = d->pending != NONE ? d->pending_page : d->page, ds = d->pending != NONE ? d->pending : d->slot;
    int mp = m->pending != NONE ? m->pending_page : m->page, ms = m->pending != NONE ? m->pending : m->slot;
    snap->drum_page = (uint8_t)(ds == NONE ? 0 : dp);
    snap->drum_slot = (uint8_t)(ds == NONE ? 0xff : ds);
    snap->melody_page = (uint8_t)(ms == NONE ? 0 : mp);
    snap->melody_slot = (uint8_t)(ms == NONE ? 0xff : ms);
    for (unsigned s = 0; s < AJB_SIDES; ++s) snap->view_page[s] = (uint8_t)e->view_page[s];
    snap->kit = (uint8_t)(e->kit_choice == NONE ? 0xff : e->kit_choice);
    snap->instrument = (uint8_t)(e->instrument_choice == NONE ? 0xff : e->instrument_choice);
    snap->effects = e->fx.on & (uint8_t)~(1U << AJB_FX_STUTTER);   /* Stutter only while held */
    snap->last_effect = e->fx.last;
    memcpy(snap->knob, e->knob, sizeof snap->knob);
    memcpy(snap->fx_amount, e->fx.amount, sizeof snap->fx_amount);
    memcpy(snap->fx_speed, e->fx.speed, sizeof snap->fx_speed);
    snap->bpm = (uint8_t)e->clock.bpm;
    snap->mood = (uint8_t)e->next_mood;
    snap->octave = (int8_t)e->octave;
    if (e->clock.running && e->clock.next_step > 0) {
        snap->clock_step = (uint8_t)((e->clock.next_step - 1U) % CYCLE);
        uint64_t offset = e->clock.elapsed - e->clock.last_start;
        snap->clock_offset = (uint16_t)(offset > 65535U ? 65535U : offset);
    }
}

static void apply_sounds(ajb_engine_t *e, const ajb_snapshot_t *snap)
{
    e->kit_choice = snap->kit == 0xff || snap->kit >= e->library->kit_count ? NONE : snap->kit;
    e->instrument_choice = snap->instrument == 0xff || snap->instrument >= e->library->instrument_count
                         ? NONE : snap->instrument;
    for (unsigned k = 0; k < AJB_EFFECTS; ++k) {
        e->fx.amount[k] = snap->fx_amount[k] > 127 ? 127 : snap->fx_amount[k];
        e->fx.speed[k] = snap->fx_speed[k] > 127 ? 127 : snap->fx_speed[k];
        ajb_fx_switch(&e->fx, k, (snap->effects >> k) & 1U && k != AJB_FX_STUTTER, e->clock.bpm, 0.0f);
    }
    if (snap->last_effect < AJB_EFFECTS) e->fx.last = snap->last_effect;
    for (unsigned k = 0; k < AJB_KNOBS; ++k) e->knob[k] = snap->knob[k] > 127 ? 127 : snap->knob[k];
    ajb_fx_swoosh(&e->fx, e->knob[AJB_KNOB_SWOOSH]);
    e->clock.bpm = snap->bpm < AJB_BPM_MIN ? AJB_BPM_MIN : snap->bpm > AJB_BPM_MAX ? AJB_BPM_MAX : snap->bpm;
    e->next_mood = snap->mood < AJB_MOODS ? snap->mood : AJB_MOOD_HAPPY;
    if (!e->clock.running) e->mood = e->next_mood;
    e->octave = snap->octave < -2 ? -2 : snap->octave > 2 ? 2 : snap->octave;
    for (unsigned s = 0; s < AJB_SIDES; ++s)
        if (snap->view_page[s] < e->library->page_count) e->view_page[s] = snap->view_page[s];
}

static void apply_snapshot(ajb_engine_t *e, const ajb_snapshot_t *snap, unsigned mode)
{
    int pages[AJB_SIDES] = {snap->drum_page, snap->melody_page};
    int slots[AJB_SIDES] = {snap->drum_slot == 0xff ? NONE : snap->drum_slot,
                            snap->melody_slot == 0xff ? NONE : snap->melody_slot};
    if (mode == SNAP_RESTORE) {
        apply_sounds(e, snap);
        for (unsigned s = 0; s < AJB_SIDES; ++s) {
            e->last_page[s] = slots[s] != NONE && loop_exists(e, s, pages[s], slots[s]) ? pages[s] : NONE;
            e->last_slot[s] = e->last_page[s] != NONE ? slots[s] : NONE;
        }
        return;
    }
    if (mode == SNAP_JAM) {
        /* Start exactly as the jam did: same loops at the same clock place. */
        for (unsigned s = 0; s < AJB_SIDES; ++s) {
            e->side[s].slot = e->side[s].pending = NONE;
            e->side[s].fade = 0;
            e->side[s].stopping = e->side[s].join_now = e->side[s].muted = 0;
            e->side[s].mute_level = AJB_FADE_SAMPLES;
            silence_side(e, s);
        }
        e->finishing = 0;
        e->clock.running = 0;
        apply_sounds(e, snap);
        if (slots[0] == NONE && slots[1] == NONE) return;
        ajb_clock_start_at(&e->clock, snap->clock_step, snap->clock_offset);
        e->half_step = e->clock.next_step;
        e->mood = e->next_mood;
        for (unsigned s = 0; s < AJB_SIDES; ++s) {
            if (slots[s] == NONE || !loop_exists(e, s, pages[s], slots[s])) continue;
            e->side[s].page = pages[s];
            e->side[s].slot = slots[s];
        }
        return;
    }
    /* A heart: swap to its loops phase-locked, sounds on the next note. */
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        side_t *side = &e->side[s];
        if (slots[s] == NONE) {
            if (side->slot != NONE) side->stopping = 1;
            side->pending = NONE;
        } else if (!(side->slot == slots[s] && side->page == pages[s] && side->pending == NONE)) {
            pick(e, s, pages[s], slots[s]);
        } else {
            side->stopping = 0;
        }
    }
    apply_sounds(e, snap);
}

void ajb_restore(ajb_engine_t *e, const ajb_snapshot_t *snap) { apply_snapshot(e, snap, SNAP_RESTORE); }

unsigned ajb_save_heart(ajb_engine_t *e)
{
    ajb_hearts_t *h = &e->hearts;
    unsigned slot = h->count < AJB_HEARTS ? h->count : (h->newest + 1U) % AJB_HEARTS;
    ajb_snapshot(e, &h->heart[slot]);
    h->heart[slot].clock_step = 0;
    h->heart[slot].clock_offset = 0;
    if (h->count < AJB_HEARTS) ++h->count;
    h->newest = (uint8_t)slot;
    h->current = (uint8_t)slot;
    return slot + 1U;
}
unsigned ajb_next_heart(ajb_engine_t *e)
{
    ajb_hearts_t *h = &e->hearts;
    if (!h->count) return 0;
    unsigned next = (h->current + 1U) % h->count;
    control(e, AJB_CTL_HEART, 0, next);
    return next + 1U;
}
const ajb_hearts_t *ajb_hearts(const ajb_engine_t *e) { return &e->hearts; }
void ajb_set_hearts(ajb_engine_t *e, const ajb_hearts_t *hearts)
{
    e->hearts = *hearts;
    if (e->hearts.count > AJB_HEARTS) e->hearts.count = AJB_HEARTS;
}

void ajb_record(ajb_engine_t *e)
{
    if (e->recording) {
        e->recording = 0;
        e->jam.length = (uint32_t)(e->now / AJB_TICK - e->jam_tick0);
        return;
    }
    e->replaying = 0;
    ajb_snapshot(e, &e->jam.start);
    e->jam.count = 0;
    e->jam.length = 0;
    e->jam_tick0 = e->now / AJB_TICK;
    e->recording = 1;
}
void ajb_replay(ajb_engine_t *e)
{
    if (e->replaying) { e->replaying = 0; return; }
    if (e->recording || !e->jam.length) return;
    apply_snapshot(e, &e->jam.start, SNAP_JAM);
    e->jam_tick0 = e->now / AJB_TICK;
    e->replay_index = 0;
    e->replaying = 1;
}
const ajb_jam_t *ajb_jam(const ajb_engine_t *e) { return &e->jam; }
void ajb_set_jam(ajb_engine_t *e, const ajb_jam_t *jam)
{
    e->jam = *jam;
    if (e->jam.count > AJB_JAM_EVENTS) e->jam.count = AJB_JAM_EVENTS;
}

/* ---- status and setup ---- */

void ajb_status(const ajb_engine_t *e, ajb_status_t *st)
{
    memset(st, 0, sizeof *st);
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        const side_t *side = &e->side[s];
        st->slot[s] = (int8_t)side->slot;
        st->page[s] = (uint8_t)(side->slot != NONE ? side->page : 0);
        st->view_page[s] = (uint8_t)e->view_page[s];
        st->pending[s] = (int8_t)side->pending;
        st->muted[s] = side->muted;
    }
    st->fill = e->fill;
    st->fast = e->fast;
    st->running = (uint8_t)e->clock.running;
    st->step = e->clock.next_step ? e->clock.next_step - 1U : 0;
    st->beat = (uint8_t)((st->step / 4U) % 4U);
    st->effects = e->fx.on;
    st->last_effect = e->fx.last;
    memcpy(st->knob, e->knob, sizeof st->knob);
    st->bpm = (uint16_t)e->clock.bpm;
    st->mood = (uint8_t)e->next_mood;
    st->kit = (uint8_t)(e->kit_applied == NONE ? 0 : e->kit_applied);
    int in = NONE;
    for (unsigned i = 0; e->instrument && i < e->library->instrument_count; ++i)
        if (e->library->instruments[i] == e->instrument) in = (int)i;
    st->instrument = (uint8_t)(e->instrument_choice != NONE ? e->instrument_choice : in == NONE ? 0 : in);
    st->octave = (int8_t)e->octave;
    st->recording = e->recording;
    st->replaying = e->replaying;
    st->finishing = e->finishing;
    st->hearts = e->hearts.count;
    st->heart = e->hearts.count ? (uint8_t)(e->hearts.current + 1U) : 0;
}

ajb_engine_t *ajb_engine_create(const ajb_library_t *library)
{
    if (!library || !library->page_count) return NULL;
    void *allocation = calloc(1, sizeof(ajb_engine_t) + 32);
    if (!allocation) return NULL;
    ajb_engine_t *e = (ajb_engine_t *)(((uintptr_t)allocation + 31U) & ~(uintptr_t)31U);
    e->allocation = allocation;
    e->fm = synth_dx7_create(DX7_VOICES, AJB_RATE, AJB_TICK);
    if (!e->fm) { free(allocation); return NULL; }
    e->library = library;
    reset_melody_engines(e);
    ajb_clock_init(&e->clock, AJB_BPM_DEFAULT);
    for (unsigned s = 0; s < AJB_SIDES; ++s) {
        e->side[s].slot = e->side[s].pending = NONE;
        e->side[s].page = e->side[s].pending_page = 0;
        e->side[s].mute_level = AJB_FADE_SAMPLES;
        e->trim[s] = 32768;
        e->last_page[s] = e->last_slot[s] = NONE;
    }
    for (unsigned k = 0; k < AJB_KNOBS; ++k) e->knob[k] = 64;
    e->chord_trim = 32768;
    e->kit_choice = e->instrument_choice = NONE;
    drum808_init(&e->drums);
    e->kit_applied = NONE;
    apply_kit(e, library->kit_count ? library->pages[0].kit : NONE);
    drum808_pan_settle(&e->drums);
    ajb_voice_reset(&e->slice_voice);
    ajb_voice_reset(&e->note_voice);
    ajb_fx_init(&e->fx);
    fm1_master_reset();
    return e;
}

void ajb_engine_destroy(ajb_engine_t *e)
{
    if (!e) return;
    synth_dx7_destroy(e->fm);
    free(e->allocation);
}

void ajb_set_trim(ajb_engine_t *e, uint32_t drums, uint32_t melody)
{
    e->trim[AJB_DRUMS] = drums;
    e->trim[AJB_MELODY] = melody;
}
void ajb_set_event_hook(ajb_engine_t *e, ajb_event_fn fn, void *context)
{
    e->hook = fn;
    e->hook_context = context;
}
uint32_t ajb_take_side_peak(ajb_engine_t *e, unsigned side)
{
    uint32_t peak = e->peak[side];
    e->peak[side] = 0;
    return peak;
}
int ajb_side_active(const ajb_engine_t *e, unsigned side)
{
    return e->side[side].slot != NONE || e->side[side].pending != NONE || e->side[side].fade > 0;
}
int ajb_clock_running(const ajb_engine_t *e) { return (int)e->clock.running; }
uint64_t ajb_now(const ajb_engine_t *e) { return e->now; }
unsigned ajb_limiter_gain_q30(void) { return fm1_master_gain(); }
