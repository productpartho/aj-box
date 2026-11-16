/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Shares FM1 Open's SEA5 tables (MIT, Dani Biro; see firmware/THIRD_PARTY.md)
 * so AJ Box samples decode exactly like the stock sample bank. */
#include "sea5.h"
#include "sampledrum.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"   /* pad maps unused here */
#include "sample_metadata.inc"
#pragma clang diagnostic pop

_Static_assert(sizeof sample_dqt / sizeof sample_dqt[0] == 16U << AJB_SEA_BITS, "SEA5 table size");

const int16_t *ajb_sea5_dqt(void) { return sample_dqt; }
