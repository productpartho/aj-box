/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * FM1 Open's stock sample format: SEA5 at 32 kHz, mono, 228 bytes per
 * 320-frame chunk. Each chunk carries its own decoder seed, so any frame can
 * be reached by decoding at most 16 groups of 20 frames from its chunk. */
#ifndef AJB_SEA5_H
#define AJB_SEA5_H
#include <stdint.h>
#include "fm1_sea.h"

enum { AJB_SEA_BITS = 5, AJB_SEA_CHUNK_BYTES = 28 + FM1_SEA_FRAMES * 5 / 8,
       AJB_SEA_GROUPS = FM1_SEA_FRAMES / FM1_SEA_GROUP };

/* The SEA5 dequantisation table from firmware/web/synth/sample_metadata.inc. */
const int16_t *ajb_sea5_dqt(void);

static inline uint32_t ajb_sea5_bytes(uint32_t frames)
{
    return (frames + FM1_SEA_FRAMES - 1U) / FM1_SEA_FRAMES * AJB_SEA_CHUNK_BYTES;
}
/* First frame of slice i when `frames` are cut into `slices` equal slices. */
static inline uint32_t ajb_slice_start(uint32_t frames, unsigned slices, unsigned i)
{
    return (uint32_t)((uint64_t)frames * i / slices);
}
#endif
