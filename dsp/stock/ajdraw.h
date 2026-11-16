/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's drawing: smooth-edged shapes and the packed pictures from
 * tools/icon_pack.pl, drawn into a strip of the 240 x 240 screen. Whole
 * numbers only, no tables bigger than a few bytes, so it suits the FM-1.
 * Positions ending in 16 are in sixteenths of a pixel. */
#ifndef AJDRAW_H
#define AJDRAW_H
#include <stdint.h>

#define AJD_RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | (b) >> 3))
/* The fixed palette (PRD "Screen and look"), in the order the pictures use. */
enum {
    AJD_PLUM = AJD_RGB(0x2A, 0x0A, 0x3D), AJD_PINK = AJD_RGB(0xFF, 0x5F, 0xA2), AJD_MAGENTA = AJD_RGB(0xE0, 0x21, 0x8A),
    AJD_GRAPE = AJD_RGB(0x7B, 0x2C, 0xBF), AJD_LILAC = AJD_RGB(0xC9, 0xA0, 0xFF), AJD_GOLD = AJD_RGB(0xFF, 0xD1, 0x66),
    AJD_CREAM = AJD_RGB(0xFF, 0xF4, 0xFB), AJD_DUSK = AJD_RGB(0x3D, 0x16, 0x56),
};

/* A strip of the screen: pixels[row * w + column] covers x..x+w, y..y+h. */
typedef struct { uint16_t *pixels; int x, y, w, h; } ajd_canvas_t;

void ajd_fill(const ajd_canvas_t *canvas, uint16_t colour);
/* alpha is 0..256. Whole pixels. */
void ajd_box(const ajd_canvas_t *canvas, int x, int y, int w, int h, int radius, uint16_t colour, unsigned alpha);
void ajd_disc(const ajd_canvas_t *canvas, int cx16, int cy16, int r16, uint16_t colour, unsigned alpha);
/* A circle line: r16 is the middle of the line. */
void ajd_ring(const ajd_canvas_t *canvas, int cx16, int cy16, int r16, int width16, uint16_t colour, unsigned alpha);
/* Part of a circle line with round ends, clockwise from the top; turns are 0..256 a full circle. */
void ajd_arc(const ajd_canvas_t *canvas, int cx16, int cy16, int r16, int width16, int from, int to, uint16_t colour);
/* A packed picture with its top-left corner at x16, y16. scale is pixels per
 * grid unit times 256; turn spins it about its middle (256 a full circle);
 * tint is the colour for the parts that leave it to the caller. */
void ajd_picture(const ajd_canvas_t *canvas, const uint8_t *art, int x16, int y16, unsigned scale, unsigned turn,
                 uint16_t tint, unsigned alpha);
/* Sine of a turn (256 a full circle), times 16384. */
int ajd_sin(unsigned turn);
#endif
