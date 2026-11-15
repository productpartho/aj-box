#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Builds AJ Box on FM1 Open's simulator, natively:
#   dsp/build/ajstock.exe        the renderer (dsp/stock/ajstock.c)
#   dsp/build/ajstock_tests.exe  the tests (dsp/stock/test_app.c)
# bash dsp/stock/build.sh [test]   "test" also runs the tests.
cd "$(dirname "$0")/../../firmware" || exit 1
CC="${AJB_CC:-$LOCALAPPDATA/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/clang.exe}"
R=..
mkdir -p $R/dsp/build
FLAGS="-std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off -DFM1_HOST_TEST -DFM1_OPEN_WEB -DAJB_FX_STOCK_SENDS
  -Wno-unused-function -Wno-array-parameter -Iweb/engine -Iweb/synth -Ithird_party/synth -Isdk/include -Iapp/synth
  -Iapp -I$R/dsp/host -I$R/dsp/include -I$R/dsp/stock -I$R/dsp/src"
SOURCES="app/*.c web/engine/*.c web/synth/*.c third_party/synth/*.c $R/dsp/host/wav.c $R/assets/loops.c
  $R/assets/samples/*.c $R/dsp/stock/ajapp.c $R/dsp/stock/ajstore.c $R/dsp/stock/oneshots.c $R/dsp/src/effects.c"
"$CC" $FLAGS $SOURCES $R/dsp/stock/ajstock.c -o $R/dsp/build/ajstock.exe -lm || exit 1
"$CC" $FLAGS $SOURCES $R/dsp/stock/test_app.c -o $R/dsp/build/ajstock_tests.exe -lm || exit 1
if [ "$1" = "test" ]; then $R/dsp/build/ajstock_tests.exe || exit 1; fi
