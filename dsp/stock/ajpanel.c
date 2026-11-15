/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's panel (see ajpanel.h). Key and button numbers are FM1 Open's:
 * keys 0..26 are F3..G5; buttons and knobs as in firmware/app/screen.c of
 * FM1 Open V14 and sdk/Simulator.js. */
#include "ajpanel.h"
#include "ajapp.h"
#include "ajscreen.h"
#include <string.h>

enum { B_OCT_DOWN, B_OCT_UP, B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, BUTTONS };
enum { E_SELECT, E_ALGORITHM, E_KNOB1, E_KNOB2, E_KNOB3, E_KNOB4, E_PRESETS };
/* Black keys (PRD "Keys"). */
enum { K_DRUM_BACK = 1, K_DRUM_NEXT = 3, K_FILL = 5, K_MUTE_DRUMS = 8, K_DICE_DRUMS = 10, K_FINISH = 13,
       K_MELODY_BACK = 15, K_MELODY_NEXT = 17, K_FAST = 20, K_MUTE_MELODY = 22, K_DICE_MELODY = 25, KEYS = 27 };
enum { LED_KEYS = 14, LEDS = 41, PARENT_HOLD_MS = 2000, BPM_STEP = 4, KNOB_STEP = 4, BLINK_MS = 120 };
#define BIT(n) (1UL << (n))

/* White keys: 8 drum loops F3..F4, 8 melody loops G4..G5. */
static const uint8_t loop_key[AJAPP_SIDES][8] = {{0, 2, 4, 6, 7, 9, 11, 12}, {14, 16, 18, 19, 21, 23, 24, 26}};

static struct {
    uint32_t keys, buttons;
    uint32_t both_since, blink_from;
    uint8_t both, home_used, save_used, octave_used, blinking;      /* blinking: side + 1 */
    uint8_t turned[AJAPP_KNOBS];        /* where each endless knob stands, 0..127 */
    uint8_t led[LEDS];
} pan;

static void led(unsigned index, int on)
{
    if (pan.led[index] == (on != 0)) return;
    pan.led[index] = on != 0;
    fm1_led_set(index, on != 0);
}

static void lights(const ajapp_status_t *st, uint32_t now)
{
    uint8_t want[LEDS];
    memset(want, 0, sizeof want);
    if (!st->parent_open) {
        for (unsigned k = 0; k < AJAPP_EFFECTS; ++k) want[B_FX + k] = st->effects >> k & 1;
        want[B_PLAY] = st->running; want[B_REC] = st->recording; want[B_ARP] = st->replaying;
        want[B_OCT_DOWN] = st->octave < 0; want[B_OCT_UP] = st->octave > 0;
        for (unsigned side = 0; side < AJAPP_SIDES; ++side) {
            /* The playing loop's key is lit while its page is the one shown. */
            if (st->slot[side] < 0 || st->slot[side] / 8 != st->view_page[side]) continue;
            int on = 1;
            if (pan.blinking == side + 1) {         /* a Dice pick blinks twice first */
                uint32_t gone = now - pan.blink_from;
                if (gone >= 4 * BLINK_MS) pan.blinking = 0;
                else on = gone / BLINK_MS & 1;
            }
            want[LED_KEYS + loop_key[side][st->slot[side] % 8]] = (uint8_t)on;
        }
        static const uint8_t held[4] = {K_FILL, K_FAST, K_MUTE_DRUMS, K_MUTE_MELODY};
        for (unsigned i = 0; i < 4; ++i) if (pan.keys & BIT(held[i])) want[LED_KEYS + held[i]] = 1;
    }
    for (unsigned i = 0; i < LEDS; ++i) led(i, want[i]);
}

void ajpanel_init(void)
{
    memset(&pan, 0, sizeof pan);
    for (unsigned k = 0; k < AJAPP_KNOBS; ++k) pan.turned[k] = 64;
    for (unsigned i = 0; i < LEDS; ++i) fm1_led_set(i, 0);
}

/* KNOB 1's middle three clicks all mean "clean" (PRD "Effects"). */
static unsigned knob_value(unsigned knob, unsigned turned)
{
    return knob == AJAPP_KNOB_SWOOSH && turned >= 64 - KNOB_STEP && turned <= 64 + KNOB_STEP ? 64 : turned;
}

static int wrap(int value, int count) { return ((value % count) + count) % count; }

void ajpanel_step(const struct fm1_controls *c, uint32_t now)
{
    ajapp_status_t st;
    ajapp_status(&st);
    const uint32_t keys = c->keys & (BIT(KEYS) - 1), buttons = c->buttons & (BIT(BUTTONS) - 1);
    const uint32_t key_down = keys & ~pan.keys, key_up = pan.keys & ~keys;
    const uint32_t down = buttons & ~pan.buttons, up = pan.buttons & ~buttons;
    const uint32_t both = BIT(B_HOME) | BIT(B_SAVE);
    pan.keys = keys; pan.buttons = buttons;

    /* HOME + SAVE held 2 s opens parent mode (and shuts it). Neither button does its own job then. */
    if ((buttons & both) == both) {
        if (!pan.both) { pan.both = 1; pan.both_since = now; }
        pan.home_used = pan.save_used = 1;
        if (pan.both == 1 && now - pan.both_since >= PARENT_HOLD_MS) {
            pan.both = 2;
            if (st.parent_open) ajapp_parent_close(); else ajapp_parent_open();
        }
    } else pan.both = 0;

    if (st.parent_open) {
        /* SELECT picks the row, any of KNOB 1 to 4 changes it, HOME leaves. Nothing else does anything. */
        int turn = c->encoders[E_KNOB1] + c->encoders[E_KNOB2] + c->encoders[E_KNOB3] + c->encoders[E_KNOB4];
        if (c->encoders[E_SELECT]) ajapp_parent_row(c->encoders[E_SELECT] > 0 ? 1 : -1);
        if (turn) (void)ajapp_parent_change(turn);
        if ((up & BIT(B_HOME)) && !pan.home_used) ajapp_parent_close();
    } else {
        for (unsigned side = 0; side < AJAPP_SIDES; ++side)
            for (unsigned slot = 0; slot < 8; ++slot)
                if (key_down & BIT(loop_key[side][slot])) ajapp_press(side, slot);
        if (key_down & BIT(K_DRUM_BACK)) ajapp_page_step(AJAPP_DRUMS, -1);
        if (key_down & BIT(K_DRUM_NEXT)) ajapp_page_step(AJAPP_DRUMS, 1);
        if (key_down & BIT(K_MELODY_BACK)) ajapp_page_step(AJAPP_MELODY, -1);
        if (key_down & BIT(K_MELODY_NEXT)) ajapp_page_step(AJAPP_MELODY, 1);
        static const uint8_t hold_key[4] = {K_FILL, K_FAST, K_MUTE_DRUMS, K_MUTE_MELODY};
        static const uint8_t hold[4] = {AJAPP_HOLD_FILL, AJAPP_HOLD_FAST, AJAPP_HOLD_MUTE_DRUMS, AJAPP_HOLD_MUTE_MELODY};
        for (unsigned i = 0; i < 4; ++i) {
            if (key_down & BIT(hold_key[i])) ajapp_hold(hold[i], 1);
            if (key_up & BIT(hold_key[i])) ajapp_hold(hold[i], 0);
        }
        for (unsigned side = 0; side < AJAPP_SIDES; ++side)
            if (key_down & BIT(side ? K_DICE_MELODY : K_DICE_DRUMS)) {
                ajapp_dice(side);
                ajscreen_event(AJSCREEN_DICE, side, 0, now);
                pan.blinking = (uint8_t)(side + 1); pan.blink_from = now;
            }
        if (key_down & BIT(K_FINISH)) ajapp_finish();

        /* The six effect buttons: five switch, GLO (Stutter) works while held. */
        for (unsigned k = 0; k < AJAPP_FX_STUTTER; ++k)
            if (down & BIT(B_FX + k)) ajapp_effect(k, !(st.effects >> k & 1));
        if (down & BIT(B_GLO)) ajapp_hold(AJAPP_HOLD_STUTTER, 1);
        if (up & BIT(B_GLO)) ajapp_hold(AJAPP_HOLD_STUTTER, 0);

        if (down & BIT(B_PLAY)) ajapp_play_stop();
        if (down & BIT(B_REC)) (void)ajapp_record();
        if (down & BIT(B_ARP)) ajapp_replay();
        if (down & BIT(B_SEQ)) ajscreen_event(AJSCREEN_HEART, ajapp_next_heart(), 0, now);
        /* HOME and SAVE act when let go, so that holding both does neither. */
        if ((up & BIT(B_SAVE)) && !pan.save_used) ajscreen_event(AJSCREEN_HEART, ajapp_save_heart(), 0, now);
        if ((up & BIT(B_HOME)) && !pan.home_used) {
            ajapp_home();
            ajscreen_event(AJSCREEN_HOME, 0, 0, now);
            pan.turned[AJAPP_KNOB_SWOOSH] = 64;
        }

        /* OCT- and OCT+: one alone moves the melody an octave when let go; both
         * together reset it; held while ALGORITHM turns, they choose the Mood. */
        const uint32_t octs = BIT(B_OCT_DOWN) | BIT(B_OCT_UP);
        if ((down & octs) && (buttons & octs) == octs) { ajapp_set_octave(0); pan.octave_used = 1; }
        if (c->encoders[E_ALGORITHM]) {
            int turn = c->encoders[E_ALGORITHM] > 0 ? 1 : -1;
            if (buttons & octs) {
                ajapp_set_mood((unsigned)wrap((int)st.mood + turn, AJAPP_MOODS));
                ajscreen_event(AJSCREEN_MOOD, 0, 0, now);
                pan.octave_used = 1;
            } else {
                int count = (int)ajb_factory_library.instrument_count;
                int at = st.instrument == 255 ? (turn > 0 ? -1 : 0) : st.instrument;
                ajapp_set_instrument((unsigned)wrap(at + turn, count));
                ajscreen_event(AJSCREEN_INSTRUMENT, 0, 0, now);
            }
        }
        if (up & octs) {
            if (!pan.octave_used) {
                int octave = st.octave + (up & BIT(B_OCT_UP) ? 1 : -1);
                ajapp_set_octave(octave < -2 ? -2 : octave > 2 ? 2 : octave);
            }
            if (!(buttons & octs)) pan.octave_used = 0;
        }

        /* SELECT: speed. PRESETS: drum kit. KNOB 1 to 4: Swoosh, how much, how fast, balance. */
        if (c->encoders[E_SELECT]) ajapp_set_bpm((unsigned)((int)st.bpm + c->encoders[E_SELECT] * BPM_STEP < AJAPP_BPM_MIN
                                                               ? AJAPP_BPM_MIN : (int)st.bpm + c->encoders[E_SELECT] * BPM_STEP));
        if (c->encoders[E_PRESETS]) {
            int turn = c->encoders[E_PRESETS] > 0 ? 1 : -1, count = (int)ajb_factory_library.kit_count;
            int at = st.kit == 255 ? (turn > 0 ? -1 : 0) : st.kit;
            ajapp_set_kit((unsigned)wrap(at + turn, count));
            ajscreen_event(AJSCREEN_KIT, 0, 0, now);
        }
        for (unsigned k = 0; k < AJAPP_KNOBS; ++k) {
            /* Something else may have moved the setting (HOME, a heart, a replay): start from there. */
            if (knob_value(k, pan.turned[k]) != st.knob[k]) pan.turned[k] = st.knob[k];
            int steps = c->encoders[E_KNOB1 + k];
            if (!steps) continue;
            int turned = (int)pan.turned[k] + steps * KNOB_STEP;
            pan.turned[k] = (uint8_t)(turned < 0 ? 0 : turned > 127 ? 127 : turned);
            unsigned value = knob_value(k, pan.turned[k]);
            ajapp_knob(k, value);
            ajscreen_event(AJSCREEN_KNOB, k, value, now);
        }
    }
    if (up & BIT(B_HOME)) pan.home_used = 0;
    if (up & BIT(B_SAVE)) pan.save_used = 0;

    ajapp_status(&st);
    lights(&st, now);
}
