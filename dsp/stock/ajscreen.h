/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's screens (PRD "Screen and look", docs/design): the start-up
 * screen, the play screen with its effect bubbles, knob arcs and recording
 * dot, the heart, Ta-da, Mood, sleepy battery and parent mode screens. What is playing
 * comes from ajapp_status; the code that reads the keys tells the screen
 * about the things a status cannot show (a heart just saved, a knob turned).
 * Times are fm1_millis() values. Control side only. */
#ifndef AJSCREEN_H
#define AJSCREEN_H
#include <stdint.h>

enum {
    AJSCREEN_HEART,         /* a = the heart's number 1..8, or 0 when nothing was saved or found */
    AJSCREEN_MOOD,          /* Mood was pressed */
    AJSCREEN_KNOB,          /* a = AJAPP_KNOB_..., b = 0..127 */
    AJSCREEN_KIT,           /* the kit was changed */
    AJSCREEN_INSTRUMENT,    /* the instrument was changed */
    AJSCREEN_DICE,          /* a = side: its picture spins in */
    AJSCREEN_HOME,          /* back to the play screen */
};

/* Start-up: shows "AJ Box" for 1.5 s, then the play screen. */
void ajscreen_init(uint32_t now);
void ajscreen_event(unsigned event, unsigned a, unsigned b, uint32_t now);
/* bars 0..3, or AJSCREEN_BATTERY_UNKNOWN to leave the battery picture out;
 * low shows the sleepy battery once when it first becomes true. */
enum { AJSCREEN_BATTERY_UNKNOWN = 255 };
void ajscreen_battery(unsigned bars, int low, uint32_t now);
/* Two numbers parent mode shows in its bottom corner, for setting the box up:
 * the battery reading (0..1023) and how many times the sound fell behind. */
void ajscreen_readings(unsigned battery, unsigned late);
/* Redraws what changed, at most max_strips of the 15 strips (16 rows each)
 * per call. Returns 1 while more is waiting. Call about 20 times a second. */
int ajscreen_draw(uint32_t now, unsigned max_strips);
/* Repaint everything on the next draws (after something else drew on the screen). */
void ajscreen_redraw(void);
#endif
