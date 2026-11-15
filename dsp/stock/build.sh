#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Builds dsp/build/ajstock.exe: AJ Box's loop layer on FM1 Open's simulator, natively.
cd "$(dirname "$0")/../../firmware" || exit 1
CC="${AJB_CC:-$LOCALAPPDATA/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/clang.exe}"
R=..
mkdir -p $R/dsp/build
"$CC" -std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off -DFM1_HOST_TEST -DFM1_OPEN_WEB -Wno-unused-function \
  -Wno-array-parameter -Iweb/engine -Iweb/synth -Ithird_party/synth -Isdk/include -Iapp/synth -Iapp \
  -I$R/dsp/host -I$R/dsp/include -I$R/dsp/stock app/*.c web/engine/*.c web/synth/*.c third_party/synth/*.c \
  $R/dsp/host/wav.c $R/assets/loops.c $R/assets/samples/*.c $R/dsp/stock/ajstock.c -o $R/dsp/build/ajstock.exe -lm
