/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's app logic on FM1 Open's stock instrument (docs/s2-design.md):
 * what each key, button and knob in PRD "Control map" does, as calls on the
 * stock instrument (firmware/app/instrument.h). The same file is meant to
 * run on the FM-1 and in the desktop simulator. Control side only, except
 * ajapp_audio, which the audio output stage calls once per block. */
#ifndef AJAPP_H
#define AJAPP_H
#include <stdint.h>
#include "ajbox_library.h"

enum { AJAPP_DRUMS, AJAPP_MELODY, AJAPP_SIDES };
enum { AJAPP_HOLD_FILL, AJAPP_HOLD_FAST, AJAPP_HOLD_MUTE_DRUMS, AJAPP_HOLD_MUTE_MELODY, AJAPP_HOLD_STUTTER, AJAPP_HOLDS };
enum { AJAPP_FX_ECHO, AJAPP_FX_SPARKLE, AJAPP_FX_WOBBLE, AJAPP_FX_WAH, AJAPP_FX_CRUNCH, AJAPP_FX_STUTTER, AJAPP_EFFECTS };
enum { AJAPP_KNOB_SWOOSH, AJAPP_KNOB_AMOUNT, AJAPP_KNOB_SPEED, AJAPP_KNOB_BALANCE, AJAPP_KNOBS };
enum { AJAPP_MOODS = 7, AJAPP_BPM_MIN = 30, AJAPP_BPM_MAX = 200, AJAPP_BPM_DEFAULT = 110 };

/* Call once after the stock instrument has started and its sample bank is bound. */
void ajapp_init(const ajb_library_t *library);
/* Call often from the control side (every few milliseconds): it finishes
 * things that wait for a beat or a bar. */
void ajapp_tick(void);
/* The finished stereo block (signed Q23, interleaved): AJ Box's own mix effects. */
void ajapp_audio(int32_t *block, unsigned frames);

void ajapp_press(unsigned side, unsigned slot);      /* a loop key on the side's page */
void ajapp_pick(unsigned side, unsigned index);      /* page * 8 + slot */
void ajapp_page_step(unsigned side, int delta);
void ajapp_dice(unsigned side);
void ajapp_hold(unsigned hold, int down);
void ajapp_finish(void);
void ajapp_play_stop(void);
void ajapp_home(void);
void ajapp_effect(unsigned effect, int on);
void ajapp_knob(unsigned knob, unsigned value);      /* 0..127 */
void ajapp_set_bpm(unsigned bpm);
void ajapp_set_kit(unsigned kit);
void ajapp_set_instrument(unsigned instrument);
void ajapp_set_octave(int octave);                   /* -2..+2 */
void ajapp_set_mood(unsigned mood);

/* Favourites, the jam and memory (PRD "Buttons and knobs", "Playback rules" 10).
 * Each returns what the screen may show: nothing counts as saved unless the
 * flash read-back matched (build plan "Saving and update-mode rules"). */
enum { AJAPP_HEARTS = 8, AJAPP_JAM_EVENTS = 1000, AJAPP_JAM_SECONDS = 120 };
unsigned ajapp_save_heart(void);     /* SAVE: the heart's number 1..8, or 0 when it could not be saved */
unsigned ajapp_next_heart(void);     /* SEQ: jump to the next heart; its number, or 0 when there are none */
int ajapp_record(void);              /* REC: start, or stop and keep; 1 recording, 0 kept, -1 not saved */
void ajapp_replay(void);             /* ARP: play her last jam; again stops it */
int ajapp_save_memory(void);         /* what power-on brings back; 0 saved */
void ajapp_chime(void);              /* the hello chime */
/* Parent mode settings; set returns 0 when saved. */
/* fx_speed: how fast the effects go, AJAPP_SPEED_NORMAL, _SLOW or _FAST. It took the
 * place of a spare byte, so Normal is 0 and settings saved before it still read. */
enum { AJAPP_SPEED_NORMAL, AJAPP_SPEED_SLOW, AJAPP_SPEED_FAST, AJAPP_SPEEDS };
typedef struct { uint8_t default_bpm, swap_on_bar, bluetooth, fx_speed; } ajapp_parent_t;
void ajapp_parent_get(ajapp_parent_t *settings);
int ajapp_parent_set(const ajapp_parent_t *settings);
/* The parent menu (PRD "Parent mode": HOME + SAVE held 2 s opens it). Four rows:
 * default tempo, swap timing, Bluetooth, effect speed. A change is saved at once; the menu
 * shows the new value only when the save was read back. */
enum { AJAPP_PARENT_TEMPO, AJAPP_PARENT_SWAP, AJAPP_PARENT_BLUETOOTH, AJAPP_PARENT_SPEED, AJAPP_PARENT_ROWS };
enum { AJAPP_PARENT_UNCHANGED, AJAPP_PARENT_SAVED, AJAPP_PARENT_NOT_SAVED };
void ajapp_parent_open(void);
void ajapp_parent_close(void);
void ajapp_parent_row(int delta);        /* move to another row */
int ajapp_parent_change(int delta);      /* change the row's setting: tempo in steps of 5; 0 when saved */

typedef struct {
    int8_t slot[AJAPP_SIDES];           /* playing loop as page * 8 + slot, -1 none */
    uint8_t view_page[AJAPP_SIDES];
    uint8_t stopping[AJAPP_SIDES], muted[AJAPP_SIDES];
    uint8_t running, finishing, fill, fast;
    uint8_t step;                       /* 0..63 in the 4-bar cycle */
    uint8_t effects, mood, kit, instrument;
    uint8_t knob[AJAPP_KNOBS];          /* 0..127 */
    int8_t octave;
    uint16_t bpm;
    uint8_t recording, replaying;
    uint8_t hearts, heart;              /* how many are saved; the one SEQ is on, 1..8, 0 none */
    uint8_t has_jam;
    int8_t waiting[AJAPP_SIDES];        /* a loop waiting for the bar line (next-bar swaps), -1 none */
    uint8_t parent_open, parent_row, parent_result;     /* AJAPP_PARENT_UNCHANGED, _SAVED or _NOT_SAVED */
} ajapp_status_t;
void ajapp_status(ajapp_status_t *status);

/* Tuning aid: a track volume for both sides in place of the table (< 0 = table). */
void ajapp_debug_volume(int volume);
#endif
