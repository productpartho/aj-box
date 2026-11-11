/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Desktop-only 16-bit PCM WAV reading and writing. */
#ifndef AJB_WAV_H
#define AJB_WAV_H
#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    unsigned channels, rate;
    uint32_t frames;
} ajb_wav_writer_t;

int ajb_wav_open(ajb_wav_writer_t *w, const char *path, unsigned channels, unsigned rate);
void ajb_wav_write(ajb_wav_writer_t *w, const int16_t *samples, uint32_t frames);
/* Signed Q23 interleaved to 16-bit with rounding; returns samples clipped. */
unsigned ajb_wav_write_q23(ajb_wav_writer_t *w, const int32_t *samples, uint32_t frames);
int ajb_wav_close(ajb_wav_writer_t *w);

/* Reads a 16-bit PCM WAV as mono (channels averaged). Caller frees *pcm. */
int ajb_wav_read_mono(const char *path, int16_t **pcm, uint32_t *frames, unsigned *rate);
#endif
