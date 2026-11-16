/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's screens (see ajscreen.h). Each call works out a small "scene"
 * (what should be on the screen right now), compares it with the one last
 * asked for, and repaints only the 16-row strips that differ. Sizes and
 * positions are read off the inline styles in the docs/design files. */
#include "ajscreen.h"
#include "ajapp.h"
#include "ajdraw.h"
#include "fm1.h"
#include <string.h>
#include "art.inc"

enum { MODE_HELLO, MODE_PLAY, MODE_HEART, MODE_FINISH, MODE_MOOD, MODE_SLEEPY, MODE_PARENT, MODE_NONE = 255 };
enum { OVER_NONE, OVER_KNOB, OVER_KIT, OVER_INSTRUMENT, OVER_TEMPO };
enum { TILE_EMPTY, TILE_PLAYING, TILE_MUTED };
enum { DECO_NONE, DECO_NOTES, DECO_SPARKLE, DECO_ECHO };
enum { STRIPS = 15, ALL = (1 << STRIPS) - 1, HELLO_MS = 1500, SHOW_MS = 1500, KNOB_MS = 1200, SPIN_MS = 400, SLEEPY_MS = 2500 };
#define PX(v) ((v) * 16)

/* Pictures in the library's order (assets/loops.c, kits.inc, instruments.inc). */
static const uint8_t *const theme_art[AJAPP_SIDES][6] = {
    {art_theme_world, art_theme_disco, art_theme_headnod, art_theme_pop, art_theme_kitchen, art_theme_sleepy},
    {art_theme_animals, art_theme_bubblegum, art_theme_space, art_theme_twinkle, art_theme_game, art_theme_dreamy},
};
static const uint8_t *const kit_art[] = {
    art_theme_pop, art_theme_disco, art_theme_headnod, art_theme_world, art_kit_spoon, art_theme_sleepy,
    art_kit_boom, art_kit_robot, art_kit_toy, art_kit_rock, art_kit_jungle, art_kit_planet,
};
static const uint8_t *const instrument_art[] = {
    art_inst_chiptune, art_inst_harp, art_inst_bells, art_inst_piano, art_inst_bass, art_inst_organ, art_inst_bloop,
    art_inst_glass, art_inst_laser, art_inst_guitar, art_inst_trumpet, art_inst_pluck, art_inst_pad, art_theme_game,
    art_inst_toypiano, art_inst_flute, art_inst_whistle, art_inst_choir, art_inst_xylophone, art_inst_shimmer,
};
static const uint8_t *const mood_art[AJAPP_MOODS] = {
    art_mood_sun, art_mood_cloud, art_mood_moon, art_kit_magic, art_mood_balloon, art_mood_lantern, art_mood_map,
};
static const uint8_t *const effect_art[AJAPP_EFFECTS] = {
    art_fx_echo, art_fx_sparkle, art_fx_wobble, art_fx_wah, art_fx_crunch, art_fx_stutter,
};
static const uint8_t *const digit_art[AJAPP_HEARTS] = {
    art_digit1, art_digit2, art_digit3, art_digit4, art_digit5, art_digit6, art_digit7, art_digit8,
};
#define COUNT(a) (sizeof (a) / sizeof (a)[0])

/* Everything is a byte so two scenes can be compared with memcmp. */
struct scene {
    uint8_t mode;
    struct { uint8_t bars, beat, pulse, speed, recording, blink; } top;
    struct { uint8_t art, state, page, lift, spin, deco, phase; } tile[AJAPP_SIDES];
    struct { uint8_t effects; } bottom;
    struct { uint8_t kind, a, b, glyph; } over;
    struct { uint8_t a, b, phase, c, d; } full;       /* the screens that take the whole display */
};

static struct {
    struct scene target;
    uint16_t pending;
    uint32_t started, mode_until, over_until, finish_until, spin_from[AJAPP_SIDES];
    uint8_t mode, mode_on, heart, over_on, over_kind, over_a, over_b, finish_on, spinning[AJAPP_SIDES];
    uint8_t effects_seen, last_effect, bars, low, readings, battery, late;
    uint16_t battery_mean;      /* the smoothed reading, in sixteenths */
    uint8_t battery_seen;
    uint8_t next, sent;         /* the strip whose turn is next; the one on its way (STRIPS: none) */
} scr;

static int before(uint32_t now, uint32_t until) { return (int32_t)(until - now) > 0; }

static void build(struct scene *s, uint32_t now)
{
    ajapp_status_t st;
    ajapp_status(&st);
    memset(s, 0, sizeof *s);
    uint8_t fresh = (uint8_t)(st.effects & ~scr.effects_seen);
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) if (fresh >> k & 1) scr.last_effect = (uint8_t)(k + 1);
    scr.effects_seen = st.effects;
    if (scr.last_effect && !(st.effects >> (scr.last_effect - 1) & 1)) scr.last_effect = 0;
    if (st.finishing) { scr.finish_on = 1; scr.finish_until = now + SHOW_MS; }
    if (scr.finish_on && !before(now, scr.finish_until)) scr.finish_on = 0;
    if (scr.mode_on && !before(now, scr.mode_until)) scr.mode_on = 0;
    if (scr.over_on && !before(now, scr.over_until)) scr.over_on = 0;

    if (now - scr.started < HELLO_MS) { s->mode = MODE_HELLO; s->full.phase = (uint8_t)((now - scr.started) / 200); return; }
    if (st.parent_open) {               /* parent mode covers everything until it is shut */
        ajapp_parent_t parent;
        ajapp_parent_get(&parent);
        s->mode = MODE_PARENT; s->full.a = st.parent_row; s->full.b = parent.default_bpm;
        s->full.phase = (uint8_t)(parent.swap_on_bar | parent.bluetooth << 1 | st.parent_result << 2 | parent.fx_speed << 4);
        if (scr.readings) { s->full.c = scr.battery; s->full.d = (uint8_t)(scr.late + 1); }
        return;
    }
    if (scr.mode_on && scr.mode == MODE_SLEEPY) { s->mode = MODE_SLEEPY; return; }
    if (scr.finish_on) { s->mode = MODE_FINISH; s->full.phase = (uint8_t)(now / 200 & 7); return; }
    if (scr.mode_on && scr.mode == MODE_HEART) {
        s->mode = MODE_HEART; s->full.a = scr.heart; s->full.b = st.hearts; s->full.phase = (uint8_t)(now / 450 & 1);
        return;
    }
    if (scr.mode_on && scr.mode == MODE_MOOD) {
        s->mode = MODE_MOOD; s->full.a = st.mood; s->full.phase = (uint8_t)(now / 250 & 7);
        return;
    }

    s->mode = MODE_PLAY;
    s->top.bars = scr.bars;
    s->top.beat = st.running ? (st.step >> 2 & 3) : 255;
    s->top.pulse = st.running && !(st.step & 3);
    unsigned bpm = st.fast ? st.bpm * 2U : st.bpm;
    s->top.speed = bpm < 90 ? 1 : bpm < 140 ? 2 : 3;
    s->top.recording = st.recording;
    s->top.blink = st.recording && (now / 550 & 1);
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) {
        /* Up on the beat, down on the next 16th: two repaints a beat. The four-step
         * hop repainted the picture all the time, which tore on the FM-1's
         * strip-by-strip screen (Partho, Nov 15, 2026). */
        static const uint8_t lift[4] = {5, 0, 0, 0};
        int playing = st.slot[side] >= 0;
        /* The picture is the page the keys will play, like the page dots (Partho, Nov 15, 2026). */
        s->tile[side].art = st.view_page[side];
        s->tile[side].state = !playing ? TILE_EMPTY : st.muted[side] ? TILE_MUTED : TILE_PLAYING;
        s->tile[side].page = st.view_page[side];
        if (st.running && s->tile[side].state == TILE_PLAYING) {
            s->tile[side].lift = lift[st.step & 3];
            if (side == AJAPP_DRUMS && (st.effects >> AJAPP_FX_ECHO & 1)) { s->tile[side].deco = DECO_ECHO; s->tile[side].phase = st.step & 7; }
            if (side == AJAPP_MELODY) {
                int sparkle = st.effects >> AJAPP_FX_SPARKLE & 1;
                s->tile[side].deco = sparkle ? DECO_SPARKLE : DECO_NOTES;
                s->tile[side].phase = sparkle ? (st.step >> 1 & 7) : (st.step & 31);
            }
        }
        if (scr.spinning[side]) {
            uint32_t gone = now - scr.spin_from[side];
            if (gone >= SPIN_MS) scr.spinning[side] = 0;
            else s->tile[side].spin = (uint8_t)((256 - gone * 256 / SPIN_MS) & 0xF8);
        }
    }
    s->bottom.effects = st.effects;
    if (scr.over_on) {
        s->over.kind = scr.over_kind; s->over.a = scr.over_a; s->over.b = scr.over_b;
        if (scr.over_kind == OVER_KIT) s->over.a = st.kit;
        if (scr.over_kind == OVER_INSTRUMENT) s->over.a = st.instrument;
        if (scr.over_kind == OVER_TEMPO) s->over.a = (uint8_t)(st.bpm > 255 ? 255 : st.bpm);
        s->over.glyph = scr.last_effect;
    }
}

/* Which strips differ between two scenes: bit n is rows 16n..16n+15. */
static unsigned differ(const struct scene *a, const struct scene *b)
{
    if (a->mode != b->mode) return ALL;
    if (a->mode != MODE_PLAY) return memcmp(&a->full, &b->full, sizeof a->full) ? ALL : 0;
    if (a->top.recording != b->top.recording) return ALL;       /* the frame round the whole screen */
    unsigned strips = 0;
    if (memcmp(&a->top, &b->top, sizeof a->top)) strips |= 0x0003;          /* rows 0..31 */
    for (unsigned side = 0; side < AJAPP_SIDES; ++side) {
        if (a->tile[side].art != b->tile[side].art || a->tile[side].state != b->tile[side].state ||
            a->tile[side].page != b->tile[side].page || a->tile[side].deco != b->tile[side].deco) {
            strips |= 0x1FFC;                                               /* the whole tile: rows 32..207 */
            continue;
        }
        /* Only what moved: fewer strips, so less of the screen is mid-repaint. */
        if (a->tile[side].lift != b->tile[side].lift || a->tile[side].spin != b->tile[side].spin)
            strips |= 0x01F0;                                               /* the picture: rows 64..143 */
        if (a->tile[side].phase != b->tile[side].phase)
            strips |= a->tile[side].deco == DECO_NOTES ? 0x001C             /* rows 32..79 */
                    : a->tile[side].deco == DECO_SPARKLE ? 0x0638           /* rows 48..95 and 144..175 */
                    : 0x03F8;                                               /* echo rings: rows 48..159 */
    }
    if (memcmp(&a->bottom, &b->bottom, sizeof a->bottom)) strips |= 0x7000; /* rows 192..239 */
    if (memcmp(&a->over, &b->over, sizeof a->over)) strips |= 0x07F0;       /* rows 64..175 */
    return strips;
}

static void picture(const ajd_canvas_t *c, const uint8_t *art, int x, int y, int size, uint16_t tint, unsigned alpha)
{
    int side = art[0] > art[1] ? art[0] : art[1];      /* size is the longer side in pixels */
    ajd_picture(c, art, PX(x), PX(y), (unsigned)(size * 256 / side), 0, tint, alpha);
}

static void paint_tile(const ajd_canvas_t *c, const struct scene *s, unsigned side)
{
    const int x = side ? 124 : 10, cx = x + 53, cy = 106 - s->tile[side].lift;
    const uint16_t main = side ? AJD_GRAPE : AJD_PINK, edge = side ? AJD_LILAC : AJD_MAGENTA;
    const unsigned state = s->tile[side].state, deco = s->tile[side].deco, phase = s->tile[side].phase;
    if (state == TILE_PLAYING) {            /* playing glows: full colour and a cream ring */
        ajd_box(c, x - 2, 38, 110, 156, 18, AJD_CREAM, 256);
        ajd_box(c, x, 40, 106, 152, 16, edge, 256);
        ajd_box(c, x + 2, 42, 102, 148, 14, main, 256);
    } else {                                /* muted or empty dims */
        ajd_box(c, x, 40, 106, 152, 16, state == TILE_MUTED ? edge : main, 256);
        ajd_box(c, x + 2, 42, 102, 148, 14, AJD_DUSK, 256);
    }
    if (deco == DECO_NOTES) {
        for (unsigned n = 0; n < 2; ++n) {
            unsigned p = (phase + n * 16) & 31;
            unsigned alpha = p < 10 ? p * 256 / 10 : (32 - p) * 256 / 22;
            ajd_picture(c, art_note, PX(n ? x + 84 : x + 10), PX(n ? 60 : 52) - (int)(p * 18 * 16 / 32),
                        n ? 219 : 256, 0, n ? AJD_GOLD : AJD_CREAM, alpha);
        }
    } else if (deco == DECO_SPARKLE) {
        static const struct { int8_t dx; uint8_t y, size, gold, delay; } spark[3] = {
            {8, 50, 14, 1, 0}, {86, 66, 10, 0, 3}, {82, 152, 12, 1, 5}};
        for (unsigned n = 0; n < 3; ++n) {
            unsigned p = (phase + spark[n].delay) & 7, dim = p < 4 ? p : 8 - p;     /* 0 bright .. 4 faint */
            picture(c, art_spark, x + spark[n].dx, spark[n].y, spark[n].size, spark[n].gold ? AJD_GOLD : AJD_CREAM, 256 - dim * 45);
        }
    } else if (deco == DECO_ECHO) {
        for (unsigned n = 0; n < 2; ++n) {
            unsigned p = (phase + n * 4) & 7;
            ajd_ring(c, PX(cx), PX(cy), PX(31) + (int)(p * PX(31) * 4 / 80), PX(2), AJD_CREAM, 205 - p * 25);
        }
    }
    ajd_disc(c, PX(cx), PX(cy), PX(32), AJD_PLUM, 256);
    ajd_picture(c, theme_art[side][s->tile[side].art % 6], PX(cx - 24), PX(cy - 24), 256, s->tile[side].spin, 0,
                state == TILE_EMPTY ? 110 : 256);
    for (int i = 0; i < 6; ++i)
        ajd_disc(c, PX(cx - 25 + 10 * i), PX(155), PX(3), i == s->tile[side].page ? AJD_CREAM : edge, 256);
}

static int text(const ajd_canvas_t *c, int x16, int y16, const char *s, unsigned scale, uint16_t colour);
static void paint_overlay(const ajd_canvas_t *c, const struct scene *s)
{
    static const uint16_t knob_colour[AJAPP_KNOBS] = {AJD_PINK, AJD_GRAPE, AJD_LILAC, AJD_MAGENTA};
    const int cx = PX(120), cy = PX(116);
    ajd_disc(c, cx, cy, PX(42), AJD_PLUM, 256);
    ajd_ring(c, cx, cy, PX(41), PX(2), AJD_DUSK, 256);
    if (s->over.kind == OVER_TEMPO) {
        /* SELECT's pop-up (Partho, Nov 15, 2026): the speed arrows, the BPM in big figures, and a
         * gold arc that fills from the slowest tempo to the fastest. */
        enum { BIG = 352 };
        const unsigned bpm = s->over.a < AJAPP_BPM_MIN ? AJAPP_BPM_MIN : s->over.a > AJAPP_BPM_MAX ? AJAPP_BPM_MAX : s->over.a;
        static const uint8_t *const arrows[3] = {art_speed1, art_speed2, art_speed3};
        char figures[4];
        unsigned n = 0;
        if (bpm >= 100) figures[n++] = (char)('0' + bpm / 100);
        figures[n++] = (char)('0' + bpm / 10 % 10); figures[n++] = (char)('0' + bpm % 10); figures[n] = 0;
        ajd_ring(c, cx, cy, PX(36), PX(5), AJD_DUSK, 256);
        if (bpm > AJAPP_BPM_MIN)
            ajd_arc(c, cx, cy, PX(36), PX(5), 0, (int)((bpm - AJAPP_BPM_MIN) * 256 / (AJAPP_BPM_MAX - AJAPP_BPM_MIN)), AJD_GOLD);
        picture(c, arrows[bpm < 90 ? 0 : bpm < 140 ? 1 : 2], 111, 90, 18, AJD_LILAC, 256);
        text(c, cx - text(0, 0, 0, figures, BIG, 0) / 2, PX(109), figures, BIG, AJD_CREAM);
        return;
    }
    if (s->over.kind != OVER_KNOB) {
        const uint8_t *art = s->over.kind == OVER_KIT ? kit_art[s->over.a % COUNT(kit_art)]
                                                      : instrument_art[s->over.a % COUNT(instrument_art)];
        ajd_picture(c, art, PX(96), PX(92), 256, 0, 0, 256);
        return;
    }
    const unsigned knob = s->over.a % AJAPP_KNOBS;
    const int value = s->over.b > 127 ? 127 : s->over.b;
    const uint8_t *glyph = art_spark;
    uint16_t tint = AJD_GOLD;
    ajd_ring(c, cx, cy, PX(32), PX(8), AJD_DUSK, 256);
    if (knob == AJAPP_KNOB_SWOOSH || knob == AJAPP_KNOB_BALANCE) {
        /* These two rest in the middle: the arc grows left or right from the top. */
        int clean = knob == AJAPP_KNOB_SWOOSH ? value >= 63 && value <= 65 : value == 64;
        uint16_t colour = knob_colour[knob];
        if (knob == AJAPP_KNOB_BALANCE) colour = value < 64 ? AJD_PINK : AJD_LILAC;      /* towards drums, towards melody */
        if (clean) ajd_disc(c, cx, cy - PX(32), PX(4), AJD_GOLD, 256);
        else ajd_arc(c, cx, cy, PX(32), PX(8), 0, (value - 64) * 2, colour);
        glyph = knob == AJAPP_KNOB_SWOOSH ? art_fx_swoosh : art_fx_balance;
        tint = AJD_CREAM;
    } else {
        if (value) ajd_arc(c, cx, cy, PX(32), PX(8), 0, value * 256 / 127, knob_colour[knob]);
        if (s->over.glyph) glyph = effect_art[(s->over.glyph - 1) % AJAPP_EFFECTS];
    }
    picture(c, glyph, 105, 101, 30, tint, 256);
}

static void paint_play(const ajd_canvas_t *c, const struct scene *s)
{
    static const struct { uint16_t bubble, glyph; } lit[AJAPP_EFFECTS] = {
        {AJD_PINK, AJD_CREAM}, {AJD_GOLD, AJD_PLUM}, {AJD_LILAC, AJD_PLUM},
        {AJD_GRAPE, AJD_CREAM}, {AJD_MAGENTA, AJD_CREAM}, {AJD_CREAM, AJD_PLUM}};
    static const uint8_t *const speed_art[3] = {art_speed1, art_speed2, art_speed3};
    /* Top strip: battery, four beat dots, speed (or the recording dot). */
    if (s->top.bars <= 3) picture(c, art_battery, 12, 13, 24, 0, 256);
    for (int i = 0; i < s->top.bars && i < 3; ++i) ajd_box(c, 16 + 5 * i, 17, 4, 6, 1, AJD_GOLD, 256);
    for (int i = 0; i < 4; ++i) {
        if (i == s->top.beat) ajd_disc(c, PX(97 + 16 * i), PX(20), s->top.pulse ? PX(7) : PX(5), AJD_GOLD, 256);
        else ajd_disc(c, PX(97 + 16 * i), PX(20), PX(4), AJD_GRAPE, 256);
    }
    if (s->top.recording) {
        unsigned alpha = s->top.blink ? 80 : 256;
        ajd_disc(c, PX(218), PX(20), PX(8), AJD_CREAM, alpha);
        ajd_disc(c, PX(218), PX(20), PX(6), AJD_MAGENTA, 256);
        if (s->top.blink) ajd_disc(c, PX(218), PX(20), PX(6), AJD_PLUM, 176);
    } else picture(c, speed_art[(s->top.speed - 1) % 3], 208, 10, 20, 0, 256);

    paint_tile(c, s, AJAPP_DRUMS);
    paint_tile(c, s, AJAPP_MELODY);

    /* Bottom strip: one bubble per effect, lit when it is on. */
    for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) {
        int cx = 30 + 36 * (int)k, on = s->bottom.effects >> k & 1;
        if (on) ajd_disc(c, PX(cx), PX(215), PX(16), AJD_CREAM, 256);
        ajd_disc(c, PX(cx), PX(215), PX(14), on ? lit[k].bubble : AJD_DUSK, 256);
        picture(c, effect_art[k], cx - 8, 207, 16, on ? lit[k].glyph : AJD_GRAPE, 256);
    }
    if (s->over.kind != OVER_NONE) paint_overlay(c, s);
    if (s->top.recording) {                 /* a magenta frame while REC is on */
        ajd_box(c, 0, 0, 240, 2, 0, AJD_MAGENTA, 256); ajd_box(c, 0, 238, 240, 2, 0, AJD_MAGENTA, 256);
        ajd_box(c, 0, 0, 2, 240, 0, AJD_MAGENTA, 256); ajd_box(c, 238, 0, 2, 240, 0, AJD_MAGENTA, 256);
    }
}

static void paint_hello(const ajd_canvas_t *c, const struct scene *s)
{
    static const int8_t bob[4] = {0, -2, -4, -2};
    static const struct { uint8_t x, y, size; uint16_t colour; } star[4] = {
        {30, 34, 22, AJD_GOLD}, {190, 46, 16, AJD_CREAM}, {196, 114, 20, AJD_GOLD}, {24, 128, 14, AJD_LILAC}};
    static const uint16_t dot[4] = {AJD_GOLD, AJD_PINK, AJD_LILAC, AJD_CREAM};
    const unsigned phase = s->full.phase;
    for (unsigned i = 0; i < 4; ++i)
        picture(c, art_spark, star[i].x, star[i].y, star[i].size, star[i].colour, (phase + i) & 1 ? 256 : 120);
    int aj = 53 + bob[phase & 3], box = 123 + bob[(phase + 3) & 3];
    ajd_picture(c, art_word_aj, PX(68), PX(aj + 4), 512, 0, AJD_MAGENTA, 256);     /* the letters' shadow */
    ajd_picture(c, art_word_aj, PX(68), PX(aj), 512, 0, AJD_PINK, 256);
    ajd_picture(c, art_word_box, PX(69), PX(box + 4), 512, 0, AJD_GRAPE, 256);
    ajd_picture(c, art_word_box, PX(69), PX(box), 512, 0, AJD_LILAC, 256);
    for (unsigned i = 0; i < 4; ++i) ajd_disc(c, PX(93 + 18 * (int)i), PX(192), (phase & 3) == i ? PX(6) : PX(4), dot[i], 256);
    picture(c, art_hello_drum, 10, 198, 32, 0, 256);
    picture(c, art_hello_notes, 198, 198, 32, 0, 256);
}

static void paint_heart(const ajd_canvas_t *c, const struct scene *s)
{
    const unsigned number = s->full.a, saved = s->full.b;
    picture(c, art_spark, 34, 40, 18, AJD_GOLD, s->full.phase ? 256 : 120);
    picture(c, art_spark, 190, 64, 14, AJD_CREAM, s->full.phase ? 120 : 256);
    /* The big heart pops a little; it stays dark and empty when nothing was saved. */
    int grow = number && s->full.phase ? 5 : 0;
    ajd_picture(c, art_heart_big, PX(60 - grow), PX(42 - grow), (unsigned)((120 + 2 * grow) * 256 / 48), 0,
                number ? AJD_PINK : AJD_DUSK, 256);
    if (number >= 1 && number <= AJAPP_HEARTS) ajd_picture(c, digit_art[number - 1], PX(111), PX(91), 256, 0, AJD_PLUM, 256);
    for (unsigned i = 0; i < AJAPP_HEARTS; ++i)
        picture(c, i + 1 == number ? art_heart_now : i < saved ? art_heart_saved : art_heart_empty, 27 + 24 * (int)i, 180, 18, 0, 256);
}

static void paint_finish(const ajd_canvas_t *c, const struct scene *s)
{
    static const struct { uint8_t x, y, w, h, r, delay; uint16_t colour; } bit[16] = {
        {17, 15, 7, 13, 2, 0, AJD_PINK}, {48, 35, 8, 8, 4, 1, AJD_GOLD}, {80, 12, 6, 12, 2, 2, AJD_LILAC},
        {118, 22, 9, 6, 2, 3, AJD_CREAM}, {152, 10, 7, 13, 2, 1, AJD_MAGENTA}, {186, 31, 8, 8, 4, 0, AJD_LILAC},
        {214, 14, 6, 11, 2, 2, AJD_GOLD}, {11, 95, 8, 8, 4, 3, AJD_CREAM}, {30, 150, 6, 12, 2, 1, AJD_LILAC},
        {15, 200, 9, 6, 2, 0, AJD_PINK}, {64, 209, 7, 7, 3, 2, AJD_GOLD}, {165, 205, 6, 12, 2, 3, AJD_CREAM},
        {202, 190, 8, 8, 4, 0, AJD_PINK}, {220, 140, 7, 13, 2, 1, AJD_GOLD}, {216, 85, 9, 6, 2, 2, AJD_LILAC},
        {118, 215, 8, 8, 4, 3, AJD_MAGENTA}};
    const unsigned phase = s->full.phase;
    for (unsigned i = 0; i < 16; ++i) {             /* confetti drifts down and back */
        unsigned p = (phase + bit[i].delay * 2) & 7, down = p < 4 ? p : 8 - p;
        ajd_box(c, bit[i].x, bit[i].y - 10 + (int)down * 30 / 4, bit[i].w, bit[i].h, bit[i].r, bit[i].colour, 256);
    }
    ajd_picture(c, art_star_face, PX(68), PX(47), 104 * 256 / 48, phase & 4 ? 250 : 0, 0, 256);
    ajd_picture(c, art_word_tada, PX(67), PX(162), 512, 0, AJD_MAGENTA, 256);
    ajd_picture(c, art_word_tada, PX(67), PX(159), 512, 0, AJD_CREAM, 256);
}

static void paint_mood(const ajd_canvas_t *c, const struct scene *s)
{
    const unsigned mood = s->full.a % AJAPP_MOODS;
    if (mood == 0) {                                /* the sun's rays turn slowly */
        ajd_picture(c, art_sun_rays, PX(64), PX(29), 112 * 256 / 48, s->full.phase * 4, 0, 256);
        ajd_picture(c, art_sun_face, PX(64), PX(29), 112 * 256 / 48, 0, 0, 256);
    } else ajd_picture(c, mood_art[mood], PX(64), PX(29), 112 * 256 / 48, 0, 0, 256);
    /* Three bubbles: the chosen mood and its neighbours. */
    unsigned first = mood == 0 ? 0 : mood >= AJAPP_MOODS - 1 ? AJAPP_MOODS - 3 : mood - 1;
    for (unsigned i = 0; i < 3; ++i) {
        int cx = 56 + 64 * (int)i;
        if (first + i == mood) ajd_disc(c, PX(cx), PX(187), PX(27), AJD_CREAM, 256);
        ajd_disc(c, PX(cx), PX(187), PX(24), AJD_DUSK, 256);
        picture(c, mood_art[first + i], cx - 16, 171, 32, 0, 256);
    }
}

static void paint_sleepy(const ajd_canvas_t *c)
{
    picture(c, art_sleepy_battery, 50, 80, 140, 0, 256);
    picture(c, art_zed, 170, 52, 16, 0, 256);
    picture(c, art_zed, 184, 40, 14, 0, 256);
    picture(c, art_zed, 160, 44, 12, 0, 256);
}

/* Words, for parent mode only: the letters it needs and the digits. */
static const char letters[] = "PDSBINOMabdefghilmnoprstuvwxEFc";
static const uint8_t *const letter_art[] = {
    art_ch_P, art_ch_D, art_ch_S, art_ch_B, art_ch_I, art_ch_N, art_ch_O, art_ch_M, art_ch_a, art_ch_b, art_ch_d, art_ch_e,
    art_ch_f, art_ch_g, art_ch_h, art_ch_i, art_ch_l, art_ch_m, art_ch_n, art_ch_o, art_ch_p, art_ch_r, art_ch_s, art_ch_t,
    art_ch_u, art_ch_v, art_ch_w, art_ch_x, art_ch_E, art_ch_F, art_ch_c,
};
static const uint8_t *const number_art[10] = {
    art_digit0, art_digit1, art_digit2, art_digit3, art_digit4, art_digit5, art_digit6, art_digit7, art_digit8, art_digit9,
};
/* Draws text with its top at y16 and returns the x16 after it; scale 256 is the
 * 16-pixel size. With no canvas it only measures. */
static int text(const ajd_canvas_t *c, int x16, int y16, const char *s, unsigned scale, uint16_t colour)
{
    for (; *s; ++s) {
        if (*s >= '0' && *s <= '9') {
            if (c) ajd_picture(c, number_art[*s - '0'], x16, y16 + (int)(scale >> 4), scale * 13 / 25, 0, colour, 256);
            x16 += (int)(19 * scale >> 5);
            continue;
        }
        const char *at = strchr(letters, *s);
        if (*s == ' ' || !at) { x16 += (int)(5 * scale >> 4); continue; }
        const uint8_t *art = letter_art[at - letters];
        if (c) ajd_picture(c, art, x16, y16, scale, 0, colour, 256);
        x16 += (int)(art[0] * scale >> 4);
    }
    return x16;
}

/* Parent mode (docs/design, Parent): a title, three rows, and whether the last change was saved. */
static void paint_parent(const ajd_canvas_t *c, const struct scene *s)
{
    static const char *const label[AJAPP_PARENT_ROWS] = {"Default tempo", "Swap timing", "Bluetooth", "Effect speed"};
    enum { SMALL = 218 };
    const unsigned bpm = s->full.b, flags = s->full.phase, result = flags >> 2 & 3, speed = flags >> 4;
    char tempo[12];
    unsigned n = 0;
    if (bpm >= 100) tempo[n++] = (char)('0' + bpm / 100);
    tempo[n++] = (char)('0' + bpm / 10 % 10); tempo[n++] = (char)('0' + bpm % 10);
    memcpy(tempo + n, " BPM", 5);
    const char *const value[AJAPP_PARENT_ROWS] = {tempo, flags & 1 ? "Next bar" : "Instant", flags & 2 ? "On" : "Off",
        speed == AJAPP_SPEED_SLOW ? "Slow" : speed == AJAPP_SPEED_FAST ? "Fast" : "Normal"};
    picture(c, art_gear, 12, 24, 20, 0, 256);
    text(c, PX(40), PX(25), "Parent mode", 256, AJD_CREAM);
    for (unsigned r = 0; r < AJAPP_PARENT_ROWS; ++r) {
        const int y = 50 + 36 * (int)r, chosen = r == s->full.a;      /* four rows: 50, 86, 122, 158 */
        ajd_box(c, 12, y, 216, 32, 10, chosen ? AJD_LILAC : AJD_DUSK, 256);
        ajd_box(c, 14, y + 2, 212, 28, 8, chosen ? AJD_DUSK : AJD_PLUM, 256);
        text(c, PX(22), PX(y + 9), label[r], SMALL, AJD_CREAM);
        const int wide = text(0, 0, 0, value[r], SMALL, 0) / 16 + 20;
        ajd_box(c, 218 - wide, y + 5, wide, 22, 11, AJD_LILAC, 256);
        text(c, PX(218 - wide + 10), PX(y + 9), value[r], SMALL, AJD_PLUM);
    }
    if (s->full.d) {                    /* b: battery, 0 to 100; l: times the sound fell behind */
        char readings[16] = "b ";
        unsigned at = 2, battery = s->full.c, late = s->full.d - 1U;
        if (battery >= 100) readings[at++] = (char)('0' + battery / 100 % 10);
        if (battery >= 10) readings[at++] = (char)('0' + battery / 10 % 10);
        readings[at++] = (char)('0' + battery % 10);
        readings[at++] = ' '; readings[at++] = 'l'; readings[at++] = ' ';
        if (late >= 100) readings[at++] = (char)('0' + late / 100);
        if (late >= 10) readings[at++] = (char)('0' + late / 10 % 10);
        readings[at++] = (char)('0' + late % 10);
        readings[at] = 0;
        text(c, PX(226) - text(0, 0, 0, readings, SMALL, 0), PX(199), readings, SMALL, AJD_GRAPE);
    }
    if (result == AJAPP_PARENT_SAVED) text(c, PX(14), PX(197), "Saved", 256, AJD_LILAC);
    else if (result == AJAPP_PARENT_NOT_SAVED) text(c, PX(14), PX(197), "Not saved", 256, AJD_PINK);
}

static void paint(const ajd_canvas_t *c, const struct scene *s)
{
    ajd_fill(c, AJD_PLUM);
    switch (s->mode) {
    case MODE_HELLO: paint_hello(c, s); break;
    case MODE_HEART: paint_heart(c, s); break;
    case MODE_FINISH: paint_finish(c, s); break;
    case MODE_MOOD: paint_mood(c, s); break;
    case MODE_SLEEPY: paint_sleepy(c); break;
    case MODE_PARENT: paint_parent(c, s); break;
    default: paint_play(c, s); break;
    }
}

void ajscreen_init(uint32_t now)
{
    memset(&scr, 0, sizeof scr);
    scr.started = now; scr.bars = 3;
    scr.target.mode = MODE_NONE; scr.pending = ALL; scr.sent = STRIPS;
}

void ajscreen_redraw(void) { scr.pending = ALL; }

void ajscreen_event(unsigned event, unsigned a, unsigned b, uint32_t now)
{
    switch (event) {
    case AJSCREEN_HEART: scr.mode = MODE_HEART; scr.heart = (uint8_t)a; scr.mode_on = 1; scr.mode_until = now + SHOW_MS; break;
    case AJSCREEN_MOOD: scr.mode = MODE_MOOD; scr.mode_on = 1; scr.mode_until = now + SHOW_MS; break;
    case AJSCREEN_KNOB: scr.over_kind = OVER_KNOB; scr.over_a = (uint8_t)a; scr.over_b = (uint8_t)b; scr.over_on = 1; scr.over_until = now + KNOB_MS; break;
    case AJSCREEN_KIT: scr.over_kind = OVER_KIT; scr.over_on = 1; scr.over_until = now + KNOB_MS; break;
    case AJSCREEN_INSTRUMENT: scr.over_kind = OVER_INSTRUMENT; scr.over_on = 1; scr.over_until = now + KNOB_MS; break;
    case AJSCREEN_TEMPO: scr.over_kind = OVER_TEMPO; scr.over_on = 1; scr.over_until = now + KNOB_MS; break;
    case AJSCREEN_DICE: if (a < AJAPP_SIDES) { scr.spinning[a] = 1; scr.spin_from[a] = now; } break;
    case AJSCREEN_HOME: scr.mode_on = scr.over_on = 0; break;
    default: break;
    }
}

void ajscreen_battery(unsigned bars, int low, uint32_t now)
{
    scr.bars = (uint8_t)(bars > 3 ? AJSCREEN_BATTERY_UNKNOWN : bars);
    if (low && !scr.low) { scr.mode = MODE_SLEEPY; scr.mode_on = 1; scr.mode_until = now + SLEEPY_MS; }
    scr.low = low != 0;
}

/* The reading in sixteenths, as 0..100 between EMPTY and FULL. */
static unsigned percent16(unsigned mean16)
{
    const unsigned empty = AJSCREEN_BATTERY_EMPTY * 16U, span = (AJSCREEN_BATTERY_FULL - AJSCREEN_BATTERY_EMPTY) * 16U;
    if (mean16 <= empty) return 0;
    return mean16 - empty >= span ? 100 : (mean16 - empty) * 100U / span;
}
unsigned ajscreen_battery_percent(unsigned adc) { return percent16((adc > 1023 ? 1023 : adc) * 16U); }

/* About 153 counts a volt if 612 is 4.0 V, so 0 is 3.4 V and 100 is 4.2 V. A wrong guess
 * errs towards a fuller picture, not a false sleepy screen. The mean settles in about 8 s;
 * two points of slack stop a bar flickering, and once low it stays low until the battery
 * has clearly risen (charging). */
void ajscreen_battery_reading(unsigned adc, uint32_t now)
{
    if (adc > 1023 || adc < AJSCREEN_BATTERY_NONE) return;      /* no battery reading: leave the picture as it is */
    static const uint8_t level[3] = {AJSCREEN_BATTERY_1, AJSCREEN_BATTERY_2, AJSCREEN_BATTERY_3};
    const int first = !scr.battery_seen;
    scr.battery_mean = (uint16_t)(first ? adc * 16U : (scr.battery_mean * 15U + adc * 16U + 8U) / 16U);
    scr.battery_seen = 1;
    const unsigned full = percent16(scr.battery_mean), slack = first ? 0 : AJSCREEN_BATTERY_SLACK;
    unsigned bars = first || scr.bars > 3 ? 0 : scr.bars;
    while (bars < 3 && full >= level[bars] + slack) ++bars;
    while (bars > 0 && full + slack < level[bars - 1]) --bars;
    const int low = scr.low && !first ? full < AJSCREEN_BATTERY_WAKE : full < AJSCREEN_BATTERY_1;
    ajscreen_battery(low ? 0 : bars, low, now);
}
/* Parent mode shows the same smoothed 0..100 the bars come from. */
void ajscreen_readings(unsigned battery, unsigned late)
{
    scr.readings = 1;
    scr.battery = (uint8_t)(scr.battery_seen ? percent16(scr.battery_mean) : ajscreen_battery_percent(battery));
    scr.late = (uint8_t)(late > 250 ? 250 : late);
}

int ajscreen_draw(uint32_t now, unsigned max_strips)
{
    static uint16_t strip[240 * 16];
    struct scene s;
    build(&s, now);
    scr.pending |= (uint16_t)differ(&scr.target, &s);
    scr.target = s;
    /* Strips take turns, carrying on from the last one sent. Starting from the
     * top every time starved the bottom of the screen whenever the tiles
     * changed faster than a whole pass (seen on the FM-1, Nov 15, 2026). */
    for (unsigned tried = 0; tried < STRIPS && max_strips; ++tried) {
        const int busy = fm1_screen_pending();
        if (busy > 0) break;                        /* the last strip is still on its way: next call */
        if (scr.sent < STRIPS) {
            if (busy < 0) scr.pending |= (uint16_t)(1U << scr.sent);     /* it never arrived: send it again */
            scr.sent = STRIPS;
        }
        const unsigned n = scr.next;
        scr.next = (uint8_t)((n + 1) % STRIPS);
        if (!(scr.pending >> n & 1)) continue;
        const ajd_canvas_t canvas = {strip, 0, (int)n * 16, 240, 16};
        paint(&canvas, &scr.target);
        if (fm1_screen_submit(0, n * 16, 240, 16, strip, 240 * 16)) { scr.next = (uint8_t)n; break; }
        scr.pending &= (uint16_t)~(1U << n);
        scr.sent = (uint8_t)n;
        --max_strips;
    }
    return scr.pending != 0;
}
