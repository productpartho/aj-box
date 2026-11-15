/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Loop manifest (PRD "Loop library and storage"): 6 pages of 8 drum and 8
 * melody loops, 12 kits, 20 instruments, 8 sampled sounds. Written in scale
 * degrees so Mood can re-key them, except four Zelda tunes with fixed notes. The data lives in
 * the loops/ folder, one file per page.
 * Drum strings: '.' rest, 'o' soft, 'x' hit, 'X' accent, one per 16th.
 * Melody characters are scale degrees: 0 C, 1 D, 2 E, 3 G, 4 A, 5 C' ...
 * '-' holds, '.' rests. Degree 0 of octave 0 is C4, of octave 1 is C5. */
#include "ajbox_target.h"
#include "ajbox_library.h"
#include <stddef.h>

/* The recorded sounds, for the old engine only. */
#ifdef AJB_NO_SAMPLES
#define AJB_SLICED(name) NULL
#else
extern const ajb_sample_t ajb_sample_musicbox, ajb_sample_kalimba, ajb_sample_la, ajb_sample_cat,
    ajb_sample_pots, ajb_sample_dhol, ajb_sample_beatbox, ajb_sample_rainstick;
#define AJB_SLICED(name) (&ajb_sample_##name)
#endif

/* Gains put each loop alone near -6 dBFS (drums) or -9 dBFS (melody) where
 * it meets the limiter, so a pair rarely needs limiting. Re-check with
 * `dsp/build/ajbox_render levels` after changing a pattern or voice. */
#define GAIN(x) ((uint16_t)((x) * 32768.0 + 0.5))

#include "loops/kits.inc"
#include "loops/instruments.inc"
#include "loops/page1.inc"
#include "loops/page2.inc"
#include "loops/page3.inc"
#include "loops/page4.inc"
#include "loops/page5.inc"
#include "loops/page6.inc"

/* ---- Fill (held drum key): a snare roll that builds into the next bar ---- */

static const ajb_drum_loop_t fill = {"Fill", 16, NULL, {
    [AJB_BD] = "X.......X.......",
    [AJB_SD] = "x.o.x.o.xoxoXxXX",
    [AJB_HT] = "....x...........",
    [AJB_LT] = "............x...",
}, GAIN(0.58)};

/* Name, drum loops, melody loops, page kit (index into kits). */
static const ajb_page_t pages[] = {
    {"Bouncy Pop / Twinkle",
     {&bounce, &stomp, &skippy, &cowbell_party, &clap_along, &hop_scotch, &big_parade, &tick_tock},
     {&star_steps, &music_box_song, &harp_hop, &bell_tower, &glass_rain, &wind_up, &tiptoe, &lullabell}, 0},
    {"Disco Dance / Bubblegum",
     {&glitter_ball, &roller_skate, &funky_chicken, &clap_hustle, &tom_tom_club, &shimmy, &mirror_ball, &cha_ching},
     {&bubble_pop, &gumdrop, &disco_strut, &fizzy_pop, &sugar_rush, &roller_rink, &sparkle_stairs, &jelly_wobble}, 1},
    {"Head-Nod / Space",
     {&boom_bap, &lazy_sunday, &head_bop, &trap_cat, &dusty, &slow_jam, &scratch, &tip_tap},
     {&laser_tag, &moon_walk, &bloop_bloop, &rocket, &robot_talk, &computer_brain, &star_drift, &ufo}, 2},
    {"Around the World / Animal Choir",
     {&dhol_party, &reggaeton, &samba, &tabla_talk, &afrobeat, &clave_walk, &goron_drums, &island},
     {&saria_song, &song_of_storms, &bird_call, &choir_rise, &frog_hop, &zeldas_lullaby, &goron_city, &owl_night}, 3},
    {"Silly Kitchen / Game On",
     {&pots_and_pans, &beatbox, &spoons, &whisk, &pop_corn, &stir_it, &clank, &dish_washer},
     {&level_one, &coin_grab, &boss_fight, &power_up, &pixel_hop, &crunch_time, &warp_zone, &high_score}, 4},
    {"Sleepy Time / Dreamy",
     {&rain_stick, &brushes, &heartbeat, &rocking_chair, &tippy_toes, &soft_shoe, &crickets, &slow_snow},
     {&kalimba_dream, &harp_glow, &toy_piano_tune, &cloud_ride, &sleepy_box, &thumb_piano, &moonbeam, &goodnight}, 5},
};

#ifndef AJB_NO_SAMPLES
static const ajb_sample_t *const samples[] = {
    &ajb_sample_musicbox, &ajb_sample_kalimba, &ajb_sample_la, &ajb_sample_cat,
    &ajb_sample_pots, &ajb_sample_dhol, &ajb_sample_beatbox, &ajb_sample_rainstick,
};
#endif

#define COUNT(a) (sizeof (a) / sizeof (a)[0])
const ajb_library_t ajb_factory_library = {
    .pages = pages, .page_count = COUNT(pages), .kits = kits, .kit_count = COUNT(kits),
    .instruments = instruments, .instrument_count = COUNT(instruments), .fill = &fill,
#ifndef AJB_NO_SAMPLES
    .samples = samples, .sample_count = COUNT(samples),
#endif
};
