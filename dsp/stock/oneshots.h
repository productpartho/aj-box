/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * The 8 synthesized one-shot sounds for the stock sampler's pads. */
#ifndef AJB_ONESHOTS_H
#define AJB_ONESHOTS_H
#include <stdint.h>

enum { AJB_ONESHOTS = 8, AJB_ONESHOT_FRAMES = 32000 };   /* at most 1 second each at 32 kHz */
/* Pad order; the loop library's pad patterns use these indexes. */
enum { AJB_PAD_DHOL_LOW, AJB_PAD_DHOL_HIGH, AJB_PAD_POT, AJB_PAD_SPOON, AJB_PAD_BOX_KICK, AJB_PAD_SHH,
       AJB_PAD_CUICA_HIGH, AJB_PAD_CUICA_LOW };
extern const char *const ajb_oneshot_names[AJB_ONESHOTS];
/* Writes pad's sound into pcm (AJB_ONESHOT_FRAMES entries); returns its length in frames. */
unsigned ajb_oneshot(unsigned pad, int16_t *pcm);
#endif
