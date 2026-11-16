#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copyright (c) 2026 Partho Ghosh.
#
# Copies AJ Box's shared code into the firmware (firmware/app/ajbox), so the
# firmware fork builds on its own with M-VAVE's unchanged sdk/Build.ps1, which
# compiles every .c under firmware/app. Edit the originals, never the copies:
#   dsp/stock, dsp/src, dsp/include, assets/loops.c, assets/loops, assets/icons
#
#   bash tools/sync_firmware.sh          copy
#   bash tools/sync_firmware.sh check    say whether the copies are up to date
cd "$(dirname "$0")/.." || exit 1
OUT=firmware/app/ajbox
FILES="dsp/stock/ajapp.c dsp/stock/ajapp.h dsp/stock/ajstore.c dsp/stock/ajstore.h dsp/stock/ajdraw.c dsp/stock/ajdraw.h
  dsp/stock/ajscreen.c dsp/stock/ajscreen.h dsp/stock/ajpanel.c dsp/stock/ajpanel.h dsp/stock/volumes.inc
  dsp/src/effects.c dsp/src/effects.h dsp/include/ajbox.h dsp/include/ajbox_library.h dsp/include/ajbox_target.h
  assets/loops.c assets/icons/art.inc
  firmware/third_party/synth/dsp_math.h firmware/third_party/synth/fixed.h"
status=0
if [ "$1" != "check" ]; then mkdir -p $OUT/loops; fi
for f in $FILES assets/loops/*.inc; do
  case $f in assets/loops/*) to=$OUT/loops/$(basename $f) ;; *) to=$OUT/$(basename $f) ;; esac
  if [ "$1" = "check" ]; then
    cmp -s "$f" "$to" || { echo "out of date: $to"; status=1; }
  else
    cmp -s "$f" "$to" || cp "$f" "$to"
  fi
done
# Anything in the folder that is not on the list is stale.
for to in $OUT/*.* $OUT/loops/*.inc; do
  [ -e "$to" ] || continue
  case $to in $OUT/loops/*) from=assets/loops/$(basename $to) ;; *) from= ; for f in $FILES; do [ "$(basename $f)" = "$(basename $to)" ] && from=$f; done ;; esac
  if [ -z "$from" ] || [ ! -e "$from" ]; then
    if [ "$1" = "check" ]; then echo "not from the originals: $to"; status=1; else rm "$to"; fi
  fi
done
exit $status
