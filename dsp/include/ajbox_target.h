/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * What a build of AJ Box leaves out. The FM-1 build has no command line of
 * its own (sdk/Build.ps1 is used unchanged), and neither has FM1 Open's browser
 * build (tools/Build-Web.ps1, FM1_OPEN_WEB), so both are recognised here.
 * The old desktop engine (dsp/build.ps1) keeps both and says so with
 * AJB_OWN_ENGINE.
 *   AJB_FX_STOCK_SENDS  Echo and Sparkle are the stock instrument's sends
 *   AJB_NO_SAMPLES      no recorded samples: the 8 sounds are one-shots in
 *                       the stock sample bank */
#ifndef AJBOX_TARGET_H
#define AJBOX_TARGET_H
#if (defined(__GCC_PI32V2__) || defined(FM1_OPEN_WEB)) && !defined(AJB_OWN_ENGINE)
#ifndef AJB_FX_STOCK_SENDS
#define AJB_FX_STOCK_SENDS 1
#endif
#ifndef AJB_NO_SAMPLES
#define AJB_NO_SAMPLES 1
#endif
#endif
#endif
