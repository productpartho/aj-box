/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * First proof for S2 (docs/s2-design.md): FM1 Open's own simulator, built
 * natively, playing patterns written through the stock instrument's calls,
 * with a drum pattern swapped while it plays. Build from firmware/:
 *
 *   clang -std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off
 *     -DFM1_HOST_TEST -DFM1_OPEN_WEB -Wno-unused-function -Wno-array-parameter
 *     -Iweb/engine -Iweb/synth -Ithird_party/synth -Isdk/include -Iapp/synth
 *     -Iapp -I../dsp/host app/*.c web/engine/*.c web/synth/*.c
 *     third_party/synth/*.c ../dsp/host/wav.c ../dsp/stock/probe.c -o probe.exe
 *
 *   probe.exe out.wav */
#include "instrument.h"
#include "drum808.h"
#include "wav.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
void sim_boot(unsigned clear);
const float *sim_render(void);
static ajb_wav_writer_t w;
static double render(unsigned blocks)
{
    double peak = 0; int16_t pcm[256];
    for (unsigned b = 0; b < blocks; ++b) {
        const float *o = sim_render();
        for (unsigned i = 0; i < 256; ++i) { double a = fabs(o[i]); if (a > peak) peak = a; double v = o[i] * 32767.0; pcm[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
        ajb_wav_write(&w, pcm, 128);
    }
    return peak;
}
int main(int argc, char **argv)
{
    sim_boot(1);
    ajb_wav_open(&w, argv[1], 2, 44100);
    struct groove_step cells[64]; memset(cells, 0, sizeof cells);
    for (unsigned s = 0; s < 64; ++s) {
        if (s % 4 == 0) cells[s].drums |= 1U << D8_BD;
        if (s % 8 == 4) cells[s].drums |= 1U << D8_SD;
        if (s % 2 == 0) cells[s].drums |= 1U << D8_CH;
        cells[s].velocity = cells[s].drums ? 100 : 0;
    }
    groove_sequence_replace(7, 0, cells, 64);
    memset(cells, 0, sizeof cells);
    static const uint8_t tune[8] = {60, 64, 67, 72, 67, 64, 60, 55};
    for (unsigned s = 0; s < 64; s += 2) { cells[s].note = tune[(s / 2) % 8]; cells[s].velocity = 100; }
    groove_sequence_replace(0, 0, cells, 64);
    const uint8_t start = 0xfa, stop = 0xfc;
    printf("post start %d\n", fm1_midi_post(&start, 1));
    printf("2 bars peak %.3f\n", render(44100 * 4 / 128));
    struct groove_view v; groove_get_view(7, 0, &v); printf("playing %u step %u tempo %u\n", v.playing, v.play_step, v.tempo);
    /* swap the drum pattern mid-bar: kick on every step */
    for (unsigned s = 0; s < 64; ++s) { cells[s].note = 0; cells[s].drums = 1U << D8_BD; cells[s].velocity = 100; }
    groove_sequence_replace(7, 0, cells, 64);
    printf("after swap peak %.3f\n", render(44100 * 2 / 128));
    fm1_midi_post(&stop, 1);
    printf("tail peak %.3f\n", render(44100 / 128));
    ajb_wav_close(&w);
    return 0;
}
