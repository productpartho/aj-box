/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's drawing (see ajdraw.h). Every shape is drawn the same way: for
 * each pixel row, four sample lines each mark which quarter-pixels the shape
 * covers; the count per pixel (0..16) is how strongly the colour is mixed in. */
#include "ajdraw.h"
#include <string.h>

enum { MAX_POINTS = 192, MAX_SUBPATHS = 16, MAX_CROSSINGS = 48, SCREEN = 240 };

static uint32_t mask[SCREEN * 4 / 32 + 1];
static uint8_t cover[SCREEN];
static int clip_lo, clip_hi;                /* sample columns the current shape may touch */

/* Mark the samples whose middles lie in xa..xb (sixteenths of a pixel). */
static void span(int xa, int xb)
{
    int a = (xa + 1) >> 2, b = (xb + 1) >> 2;
    if (a < clip_lo) a = clip_lo;
    if (b > clip_hi) b = clip_hi;
    while (a < b) {
        int word = a >> 5, bit = a & 31, n = 32 - bit;
        if (n > b - a) n = b - a;
        mask[word] |= (n == 32 ? 0xFFFFFFFFU : ((1U << n) - 1U)) << bit;
        a += n;
    }
}

static unsigned root(uint32_t v)
{
    uint32_t r = 0, bit = 1UL << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (unsigned)r;
}

typedef void (*emit_t)(const void *shape, int y16);

/* Draw one shape inside the pixel box x0..x1, y0..y1: emit marks one sample line. */
static void raster(const ajd_canvas_t *c, int x0, int y0, int x1, int y1, emit_t emit, const void *shape,
                   uint16_t colour, unsigned alpha)
{
    static const uint8_t ones[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
    if (x0 < c->x) x0 = c->x;
    if (y0 < c->y) y0 = c->y;
    if (x1 > c->x + c->w) x1 = c->x + c->w;
    if (y1 > c->y + c->h) y1 = c->y + c->h;
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN) x1 = SCREEN;
    if (x0 >= x1 || y0 >= y1 || !alpha) return;
    clip_lo = x0 * 4; clip_hi = x1 * 4;
    const uint32_t fg_rb = colour & 0xF81FU, fg_g = colour & 0x07E0U;
    for (int y = y0; y < y1; ++y) {
        memset(cover + x0, 0, (size_t)(x1 - x0));
        for (int k = 0; k < 4; ++k) {
            for (int w = x0 >> 3; w <= (x1 - 1) >> 3; ++w) mask[w] = 0;
            emit(shape, y * 16 + k * 4 + 2);
            for (int x = x0; x < x1; ++x) {
                uint32_t word = mask[x >> 3];
                if (!word) { x |= 7; continue; }
                cover[x] = (uint8_t)(cover[x] + ones[word >> ((x & 7) * 4) & 15]);
            }
        }
        uint16_t *row = c->pixels + (y - c->y) * c->w - c->x;
        for (int x = x0; x < x1; ++x) {
            if (!cover[x]) continue;
            uint32_t a = cover[x] * alpha >> 4;
            if (a >= 256) { row[x] = colour; continue; }
            uint32_t d = row[x];
            row[x] = (uint16_t)(((fg_rb * a + (d & 0xF81FU) * (256 - a)) >> 8 & 0xF81FU) |
                                ((fg_g * a + (d & 0x07E0U) * (256 - a)) >> 8 & 0x07E0U));
        }
    }
}

void ajd_fill(const ajd_canvas_t *c, uint16_t colour)
{
    for (int i = 0; i < c->w * c->h; ++i) c->pixels[i] = colour;
}

/* Boxes, discs and rings. */
struct round { int x, y, w, h, r, inner; };     /* sixteenths; for a disc x,y is the middle */

static void emit_box(const void *shape, int y)
{
    const struct round *b = shape;
    if (y < b->y || y >= b->y + b->h) return;
    int inset = 0, dy = 0;
    if (y < b->y + b->r) dy = b->y + b->r - y;
    else if (y > b->y + b->h - b->r) dy = y - (b->y + b->h - b->r);
    if (dy) inset = b->r - (int)root((uint32_t)(b->r * b->r - dy * dy));
    span(b->x + inset, b->x + b->w - inset);
}
void ajd_box(const ajd_canvas_t *c, int x, int y, int w, int h, int radius, uint16_t colour, unsigned alpha)
{
    struct round b = {x * 16, y * 16, w * 16, h * 16, radius * 16, 0};
    raster(c, x, y, x + w, y + h, emit_box, &b, colour, alpha);
}

static void emit_disc(const void *shape, int y)
{
    const struct round *d = shape;
    int dy = y - d->y;
    if (dy < 0) dy = -dy;
    if (dy >= d->r) return;
    int half = (int)root((uint32_t)(d->r * d->r - dy * dy));
    if (dy >= d->inner) { span(d->x - half, d->x + half); return; }
    int hole = (int)root((uint32_t)(d->inner * d->inner - dy * dy));
    span(d->x - half, d->x - hole);
    span(d->x + hole, d->x + half);
}
static void round_shape(const ajd_canvas_t *c, int cx, int cy, int r, int inner, uint16_t colour, unsigned alpha)
{
    if (r <= 0) return;
    struct round d = {cx, cy, 0, 0, r, inner < 0 ? 0 : inner};
    raster(c, (cx - r) >> 4, (cy - r) >> 4, ((cx + r) >> 4) + 1, ((cy + r) >> 4) + 1, emit_disc, &d, colour, alpha);
}
void ajd_disc(const ajd_canvas_t *c, int cx16, int cy16, int r16, uint16_t colour, unsigned alpha)
{
    round_shape(c, cx16, cy16, r16, 0, colour, alpha);
}
void ajd_ring(const ajd_canvas_t *c, int cx16, int cy16, int r16, int width16, uint16_t colour, unsigned alpha)
{
    round_shape(c, cx16, cy16, r16 + width16 / 2, r16 - width16 / 2, colour, alpha);
}

/* Outlines: points in sixteenths of a pixel, in subpaths. */
static struct {
    int32_t x[MAX_POINTS], y[MAX_POINTS];
    int16_t nx[MAX_POINTS], ny[MAX_POINTS];     /* stroke: half-width across the line that starts at each point */
    uint8_t first[MAX_SUBPATHS], count[MAX_SUBPATHS], closed[MAX_SUBPATHS];
    unsigned subpaths, points;
    int half;                                   /* stroke half-width */
} path;

static int crossing(int xa, int ya, int xb, int yb, int y, int *x)
{
    if ((ya <= y) == (yb <= y)) return 0;
    *x = xa + (y - ya) * (xb - xa) / (yb - ya);
    return 1;
}

/* Fill: the gaps between pairs of edge crossings. */
static void emit_fill(const void *unused, int y)
{
    (void)unused;
    int xs[MAX_CROSSINGS], n = 0;
    for (unsigned s = 0; s < path.subpaths; ++s) {
        unsigned first = path.first[s], count = path.count[s];
        for (unsigned i = 0; i < count && n < MAX_CROSSINGS; ++i) {
            unsigned a = first + i, b = first + (i + 1) % count;
            int x;
            if (!crossing(path.x[a], path.y[a], path.x[b], path.y[b], y, &x)) continue;
            int at = n++;
            while (at > 0 && xs[at - 1] > x) { xs[at] = xs[at - 1]; --at; }
            xs[at] = x;
        }
    }
    for (int i = 0; i + 1 < n; i += 2) span(xs[i], xs[i + 1]);
}

/* Stroke: a slab for each line and a disc at each point. */
static void emit_stroke(const void *unused, int y)
{
    (void)unused;
    const int half = path.half;
    for (unsigned s = 0; s < path.subpaths; ++s) {
        unsigned first = path.first[s], count = path.count[s];
        unsigned lines = path.closed[s] ? count : count - 1;
        for (unsigned i = 0; i < count; ++i) {
            unsigned a = first + i;
            int dy = y - path.y[a];
            if (dy < 0) dy = -dy;
            if (dy < half) {
                int w = (int)root((uint32_t)(half * half - dy * dy));
                span(path.x[a] - w, path.x[a] + w);
            }
            if (i >= lines) continue;
            unsigned b = first + (i + 1) % count;
            int lo = path.y[a] < path.y[b] ? path.y[a] : path.y[b], hi = path.y[a] + path.y[b] - lo;
            if (y < lo - half || y > hi + half || (!path.nx[a] && !path.ny[a])) continue;
            int px[4] = {path.x[a] + path.nx[a], path.x[b] + path.nx[a], path.x[b] - path.nx[a], path.x[a] - path.nx[a]};
            int py[4] = {path.y[a] + path.ny[a], path.y[b] + path.ny[a], path.y[b] - path.ny[a], path.y[a] - path.ny[a]};
            int left = 0, right = 0, found = 0, x;
            for (unsigned e = 0; e < 4; ++e) {
                if (!crossing(px[e], py[e], px[(e + 1) & 3], py[(e + 1) & 3], y, &x)) continue;
                if (!found || x < left) left = x;
                if (!found || x > right) right = x;
                found = 1;
            }
            if (found) span(left, right);
        }
    }
}

static void path_box(int margin, int *x0, int *y0, int *x1, int *y1)
{
    int lx = path.x[0], hx = lx, ly = path.y[0], hy = ly;
    for (unsigned i = 1; i < path.points; ++i) {
        if (path.x[i] < lx) lx = path.x[i];
        if (path.x[i] > hx) hx = path.x[i];
        if (path.y[i] < ly) ly = path.y[i];
        if (path.y[i] > hy) hy = path.y[i];
    }
    *x0 = (lx - margin) >> 4; *y0 = (ly - margin) >> 4; *x1 = ((hx + margin) >> 4) + 1; *y1 = ((hy + margin) >> 4) + 1;
}

static void stroke_path(const ajd_canvas_t *c, int width16, uint16_t colour, unsigned alpha)
{
    int x0, y0, x1, y1;
    path.half = width16 / 2;
    if (path.half < 1 || !path.points) return;
    path_box(path.half, &x0, &y0, &x1, &y1);
    if (x1 <= c->x || x0 >= c->x + c->w || y1 <= c->y || y0 >= c->y + c->h) return;
    for (unsigned s = 0; s < path.subpaths; ++s)
        for (unsigned i = 0; i < path.count[s]; ++i) {
            unsigned a = path.first[s] + i, b = path.first[s] + (i + 1) % path.count[s];
            int dx = path.x[b] - path.x[a], dy = path.y[b] - path.y[a];
            int length = (int)root((uint32_t)(dx * dx + dy * dy));
            path.nx[a] = (int16_t)(length ? -dy * path.half / length : 0);
            path.ny[a] = (int16_t)(length ? dx * path.half / length : 0);
        }
    raster(c, x0, y0, x1, y1, emit_stroke, 0, colour, alpha);
}

int ajd_sin(unsigned turn)
{
    static const uint16_t quarter[17] = {0, 1606, 3196, 4756, 6270, 7723, 9102, 10394, 11585, 12665, 13623, 14449,
                                         15137, 15679, 16069, 16305, 16384};
    unsigned t = turn & 255, q = t & 63;
    if (t & 64) q = 64 - q;
    int v = quarter[q >> 2];
    if (q & 3) v += (quarter[(q >> 2) + 1] - v) * (int)(q & 3) / 4;
    return t & 128 ? -v : v;
}

void ajd_arc(const ajd_canvas_t *c, int cx16, int cy16, int r16, int width16, int from, int to, uint16_t colour)
{
    if (to < from) { int swap = from; from = to; to = swap; }
    unsigned n = 0;
    for (int t = from; n < MAX_POINTS; t += 4) {
        if (t > to) t = to;
        path.x[n] = cx16 + (r16 * ajd_sin((unsigned)t) >> 14);
        path.y[n] = cy16 - (r16 * ajd_sin((unsigned)t + 64) >> 14);
        ++n;
        if (t == to) break;
    }
    path.first[0] = 0; path.count[0] = (uint8_t)n; path.closed[0] = 0; path.subpaths = 1; path.points = n;
    stroke_path(c, width16, colour, 256);
}

void ajd_picture(const ajd_canvas_t *c, const uint8_t *art, int x16, int y16, unsigned scale, unsigned turn,
                 uint16_t tint, unsigned alpha)
{
    static const uint16_t palette[8] = {AJD_PLUM, AJD_PINK, AJD_MAGENTA, AJD_GRAPE, AJD_LILAC, AJD_GOLD, AJD_CREAM, AJD_DUSK};
    const int w = art[0], h = art[1];
    const int cx = x16 + (int)(w * scale >> 5), cy = y16 + (int)(h * scale >> 5);
    {   /* Leave early when the picture is nowhere near this strip. */
        int reach = (int)((w > h ? w : h) * scale >> 4) + 64;       /* its longest side, over half the diagonal */
        if (cx + reach < c->x * 16 || cx - reach > (c->x + c->w) * 16 || cy + reach < c->y * 16 ||
            cy - reach > (c->y + c->h) * 16) return;
    }
    const int sine = ajd_sin(turn), cosine = ajd_sin(turn + 64);
    const uint8_t *p = art + 3;
    for (unsigned shapes = art[2]; shapes; --shapes) {
        unsigned fill = p[0] >> 4, stroke = p[0] & 15, width = p[1], subpaths = p[2];
        p += 3;
        path.subpaths = 0; path.points = 0;
        for (unsigned s = 0; s < subpaths; ++s) {
            unsigned count = p[0] & 127, closed = p[0] >> 7;
            ++p;
            int keep = path.subpaths < MAX_SUBPATHS && path.points + count <= MAX_POINTS;
            if (keep) {
                path.first[path.subpaths] = (uint8_t)path.points; path.count[path.subpaths] = (uint8_t)count;
                path.closed[path.subpaths++] = (uint8_t)closed;
            }
            for (unsigned i = 0; i < count; ++i, p += 2) {
                if (!keep) continue;
                int du = p[0] - w * 2, dv = p[1] - h * 2;       /* quarter units from the middle */
                path.x[path.points] = cx + (int)((((du * cosine - dv * sine) >> 4) * (int)scale) >> 16);
                path.y[path.points++] = cy + (int)((((du * sine + dv * cosine) >> 4) * (int)scale) >> 16);
            }
        }
        if (!path.points) continue;
        if (fill != 15) {
            int x0, y0, x1, y1;
            path_box(0, &x0, &y0, &x1, &y1);
            raster(c, x0, y0, x1, y1, emit_fill, 0, fill < 8 ? palette[fill] : tint, alpha);
        }
        if (stroke != 15 && width) stroke_path(c, (int)(width * scale >> 6), stroke < 8 ? palette[stroke] : tint, alpha);
    }
}
