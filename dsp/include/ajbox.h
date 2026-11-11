/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * AJ Box sound engine: one clock, one drum loop + one melody loop,
 * phase-locked swaps and an always-on output limiter (PRD "Playback rules"),
 * plus every control in PRD "Control map". The firmware maps keys, buttons
 * and knobs to these calls and draws the screen from ajb_status(). */
#ifndef AJBOX_H
#define AJBOX_H
#include <stddef.h>
#include <stdint.h>
#include "ajbox_library.h"

enum { AJB_RATE = 44100, AJB_BLOCK = 128, AJB_TICK = 64 };
enum { AJB_DRUMS, AJB_MELODY, AJB_SIDES };
/* PRD "Playback rules" 1: 30 to 200 BPM in 2 BPM steps on SELECT. */
enum { AJB_BPM_MIN = 30, AJB_BPM_MAX = 200, AJB_BPM_DEFAULT = 110, AJB_BPM_STEP = 2 };
/* A swap pressed within this long after a 16th step starts joins on that
 * step; later presses join on the next step. */
enum { AJB_GRACE_SAMPLES = AJB_RATE * 25 / 1000 };
enum { AJB_FADE_SAMPLES = AJB_RATE / 100 };   /* 10 ms stop and mute fade */
/* fm1_master_process128 ceiling, -0.5 dBFS of signed Q23. */
#define AJB_OUTPUT_CEILING 7919643

typedef struct ajb_engine ajb_engine_t;

/* Every fired loop step (kind STEP), and moments the screen animates. */
enum { AJB_EVENT_STEP, AJB_EVENT_FINISH, AJB_EVENT_DICE, AJB_EVENT_JAM_END };
typedef struct {
    uint64_t sample;        /* engine sample where it happened */
    uint64_t exact;         /* exact sample of the clock step (or the press) */
    uint64_t global_step;
    unsigned side, slot, loop_step;
    unsigned kind, page;
} ajb_event_t;
typedef void (*ajb_event_fn)(void *context, const ajb_event_t *event);

/* Create before audio starts; allocates once (the DX7 core uses calloc). */
ajb_engine_t *ajb_engine_create(const ajb_library_t *library);
void ajb_engine_destroy(ajb_engine_t *engine);

/* ---- controls (PRD "Control map"); each takes effect at the next tick ---- */

/* Every control is one of these, so a jam can record and replay them. */
enum {
    AJB_CTL_PRESS,        /* a side, b slot on the side's current page */
    AJB_CTL_PICK,         /* a side, b page * 8 + slot: an exact loop (Dice, hearts) */
    AJB_CTL_PAGE,         /* a side, b page to show (Back and Next) */
    AJB_CTL_HOLD,         /* a hold, b 1 down / 0 up */
    AJB_CTL_FINISH,
    AJB_CTL_PLAY_STOP,
    AJB_CTL_HOME,
    AJB_CTL_EFFECT,       /* a effect, b 1 on / 0 off */
    AJB_CTL_KNOB,         /* a knob, b value 0..127 */
    AJB_CTL_BPM,          /* b BPM */
    AJB_CTL_KIT,          /* b kit index */
    AJB_CTL_INSTRUMENT,   /* b instrument index */
    AJB_CTL_OCTAVE,       /* b octave offset + 2 (0..4) */
    AJB_CTL_MOOD,         /* b mood */
    AJB_CTL_HEART,        /* b heart index: a jam replays SEQ with today's hearts */
    AJB_CTLS
};
typedef struct { uint8_t type, a, b; } ajb_control_t;
void ajb_control(ajb_engine_t *engine, ajb_control_t control);

/* Loop keys: tap to start or swap; the playing key again stops that side
 * on the next beat (PRD "Playback rules" 2 to 4). */
void ajb_press(ajb_engine_t *engine, unsigned side, unsigned slot);
/* Back (-1) and Next (+1): which page the side's 8 keys play from. The
 * playing loop keeps going. Pages wrap. */
void ajb_page_step(ajb_engine_t *engine, unsigned side, int delta);
/* Dice: a surprise loop from any page on that side, never the playing one. */
void ajb_dice(ajb_engine_t *engine, unsigned side);

/* Keys that act only while held (PRD "Keys"). Fill: a drum fill. Fast: the
 * melody at double speed. Mute: that side drops out, and comes back on the
 * next beat after release. Stutter is an effect but also held. */
enum { AJB_HOLD_FILL, AJB_HOLD_FAST, AJB_HOLD_MUTE_DRUMS, AJB_HOLD_MUTE_MELODY, AJB_HOLD_STUTTER, AJB_HOLDS };
void ajb_hold(ajb_engine_t *engine, unsigned hold, int down);
/* Finish: on the next bar a crash and the melody's final chord, then stop. */
void ajb_finish(ajb_engine_t *engine);
/* PLAY/STOP: stop both sides at once (10 ms fade); again resumes the last pair. */
void ajb_play_stop(ajb_engine_t *engine);
/* HOME: every effect off, the Swoosh filter back to the middle. */
void ajb_home(ajb_engine_t *engine);

/* Effects (PRD "Effects"): the 6 top-row buttons toggle these. KNOB 2 and
 * KNOB 3 shape the one switched on last. */
enum { AJB_FX_ECHO, AJB_FX_SPARKLE, AJB_FX_WOBBLE, AJB_FX_WAH, AJB_FX_CRUNCH, AJB_FX_STUTTER, AJB_EFFECTS };
void ajb_effect(ajb_engine_t *engine, unsigned effect, int on);
/* KNOB 1 Swoosh filter (middle 3 values clean), 2 how much, 3 how fast,
 * 4 drums/melody balance. Values 0..127. */
enum { AJB_KNOB_SWOOSH, AJB_KNOB_AMOUNT, AJB_KNOB_SPEED, AJB_KNOB_BALANCE, AJB_KNOBS };
void ajb_knob(ajb_engine_t *engine, unsigned knob, unsigned value);

void ajb_set_bpm(ajb_engine_t *engine, unsigned bpm);
unsigned ajb_bpm(const ajb_engine_t *engine);
/* PRESETS and ALGORITHM: the sound changes on the next note (rule 9).
 * A sampled drum loop keeps its own sound. Picking a new melody loop goes
 * back to that loop's own instrument. */
void ajb_set_kit(ajb_engine_t *engine, unsigned kit);
void ajb_set_instrument(ajb_engine_t *engine, unsigned instrument);
/* OCT- and OCT+: melody octave offset -2..+2; 0 resets. */
void ajb_set_octave(ajb_engine_t *engine, int octave);
/* PRD "Buttons and knobs", Mood: seven 5-note scales. While the clock runs
 * the change lands on the next bar line; sounding notes keep their pitch. */
enum { AJB_MOOD_HAPPY, AJB_MOOD_DREAMY, AJB_MOOD_SPOOKY, AJB_MOOD_MAGIC, AJB_MOOD_FLOATY,
       AJB_MOOD_FARAWAY, AJB_MOOD_ADVENTURE, AJB_MOODS };
void ajb_set_mood(ajb_engine_t *engine, unsigned mood);
unsigned ajb_mood(const ajb_engine_t *engine);   /* the mood notes play in now */

/* Parent mode: swaps phase-locked on the next step (default) or on the
 * next bar for a classic DJ feel. */
enum { AJB_SWAP_STEP, AJB_SWAP_BAR };
void ajb_set_swap_mode(ajb_engine_t *engine, unsigned mode);

/* Start-up: the 1-second hello chime, at low volume. */
void ajb_chime(ajb_engine_t *engine);

/* ---- favourites, jams and memory (PRD "Buttons and knobs", rule 10) ---- */

/* Everything a heart, a jam start or power-off restores: 32 bytes. */
typedef struct {
    uint8_t drum_page, drum_slot;       /* 0xff slot = silent */
    uint8_t melody_page, melody_slot;
    uint8_t view_page[AJB_SIDES];       /* pages the keys show */
    uint8_t kit, instrument;            /* 0xff = the loop's own */
    uint8_t effects, last_effect;       /* bit per effect */
    uint8_t knob[AJB_KNOBS];
    uint8_t fx_amount[AJB_EFFECTS], fx_speed[AJB_EFFECTS];
    uint8_t bpm, mood;
    int8_t octave;
    uint8_t clock_step;                 /* jams: step within the 4-bar cycle */
    uint16_t clock_offset;              /* jams: samples into that step */
} ajb_snapshot_t;
void ajb_snapshot(const ajb_engine_t *engine, ajb_snapshot_t *snapshot);
/* Power-on restore: sounds, pages, tempo and effects; the last pair waits
 * for PLAY/STOP instead of starting. */
void ajb_restore(ajb_engine_t *engine, const ajb_snapshot_t *snapshot);

/* SAVE and SEQ: 8 hearts; a new one replaces the oldest; each SEQ tap jumps
 * to the next heart. ajb_save_heart returns the heart's number 1..8. */
enum { AJB_HEARTS = 8 };
unsigned ajb_save_heart(ajb_engine_t *engine);
unsigned ajb_next_heart(ajb_engine_t *engine);   /* 0 when none saved */
typedef struct { ajb_snapshot_t heart[AJB_HEARTS]; uint8_t count, newest, current; } ajb_hearts_t;
const ajb_hearts_t *ajb_hearts(const ajb_engine_t *engine);
void ajb_set_hearts(ajb_engine_t *engine, const ajb_hearts_t *hearts);

/* REC and ARP: record up to 2 minutes of controls; replay her last jam.
 * Events are 4 bytes: tick (64 samples) since the start, type, a, b. */
enum { AJB_JAM_EVENTS = 1024, AJB_JAM_TICKS = 2 * 60 * AJB_RATE / AJB_TICK };
typedef struct {
    ajb_snapshot_t start;
    uint32_t event[AJB_JAM_EVENTS];
    uint16_t count;
    uint32_t length;                    /* ticks */
} ajb_jam_t;
void ajb_record(ajb_engine_t *engine);   /* REC: start, or stop and keep */
void ajb_replay(ajb_engine_t *engine);   /* ARP: play the jam back; again stops */
const ajb_jam_t *ajb_jam(const ajb_engine_t *engine);
void ajb_set_jam(ajb_engine_t *engine, const ajb_jam_t *jam);

/* ---- screen ---- */

typedef struct {
    int8_t slot[AJB_SIDES];             /* playing, -1 none */
    uint8_t page[AJB_SIDES];            /* page of the playing loop */
    uint8_t view_page[AJB_SIDES];       /* page the keys show (page dots) */
    int8_t pending[AJB_SIDES];
    uint8_t muted[AJB_SIDES], fill, fast;
    uint8_t running, beat;              /* beat 0..3 within the bar */
    uint64_t step;
    uint8_t effects, last_effect;
    uint8_t knob[AJB_KNOBS];
    uint16_t bpm;
    uint8_t mood, kit, instrument;
    int8_t octave;
    uint8_t recording, replaying, finishing;
    uint8_t hearts, heart;
} ajb_status_t;
void ajb_status(const ajb_engine_t *engine, ajb_status_t *status);

/* ---- audio ---- */

/* AJB_BLOCK interleaved stereo frames of signed Q23, limited. */
void ajb_render(ajb_engine_t *engine, int32_t out[AJB_BLOCK * 2]);

/* Debug and test: scale both sides (Q15, 32768 = unity) before the limiter,
 * on top of the balance knob. */
void ajb_set_trim(ajb_engine_t *engine, uint32_t drums_q15, uint32_t melody_q15);
void ajb_set_event_hook(ajb_engine_t *engine, ajb_event_fn fn, void *context);
/* Debug: largest |sample| on a side's bus (Q24, before trim and limiter)
 * since the last call; resets it. */
uint32_t ajb_take_side_peak(ajb_engine_t *engine, unsigned side);
int ajb_side_active(const ajb_engine_t *engine, unsigned side);
int ajb_clock_running(const ajb_engine_t *engine);
uint64_t ajb_now(const ajb_engine_t *engine);
unsigned ajb_limiter_gain_q30(void);
#endif
