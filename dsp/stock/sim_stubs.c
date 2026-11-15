/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * Desktop only. FM1 Open's simulator (firmware/web/engine/Simulator.c) has
 * browser entry points that reach into FM1 Open's own editor, projects and
 * voice library. AJ Box's firmware has none of those, and neither the
 * renderer nor the tests call these entry points, so they are empty here
 * just to let the simulator link. None of this goes on the FM-1. */
#define EMPTY(name) int name(void) { return 0; }
EMPTY(groove_factory_build)
EMPTY(groove_factory_name)
EMPTY(groove_fm_library_read)
EMPTY(groove_fm_library_write)
EMPTY(groove_project_export)
EMPTY(groove_project_import)
EMPTY(groove_project_revision)
EMPTY(groove_screen_open)
EMPTY(groove_screen_preset)
EMPTY(groove_screen_preset_position)
EMPTY(groove_screen_snapshot)
EMPTY(groove_screen_tap)
EMPTY(groove_screen_web_adjust)
EMPTY(groove_screen_web_context)
EMPTY(groove_screen_web_fine)
EMPTY(groove_screen_web_first)
EMPTY(groove_screen_web_knobs)
EMPTY(groove_screen_web_mask)
EMPTY(groove_screen_web_step)
EMPTY(groove_screen_web_target_count)
EMPTY(groove_screen_web_targets)
EMPTY(groove_settings_midi)
