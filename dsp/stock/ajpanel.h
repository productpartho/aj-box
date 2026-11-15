/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's panel: what each of the FM-1's keys, buttons and knobs does
 * (PRD "Control map"), and the key and button lights. It turns one reading
 * of the controls into calls on ajapp and ajscreen. Control side only. */
#ifndef AJPANEL_H
#define AJPANEL_H
#include <stdint.h>
#include "fm1.h"

void ajpanel_init(void);
/* Call with every fresh reading of the controls. */
void ajpanel_step(const struct fm1_controls *controls, uint32_t now);
#endif
