/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh. */
#include "wav.h"
#include <stdlib.h>
#include <string.h>

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xffffU); put16(p + 2, v >> 16); }
static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static unsigned get16(const uint8_t *p) { return p[0] | p[1] << 8; }

static void header(ajb_wav_writer_t *w)
{
    uint8_t h[44];
    uint32_t data = w->frames * w->channels * 2U;
    memcpy(h, "RIFF", 4); put32(h + 4, 36U + data); memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16); put16(h + 20, 1); put16(h + 22, w->channels);
    put32(h + 24, w->rate); put32(h + 28, w->rate * w->channels * 2U);
    put16(h + 32, w->channels * 2U); put16(h + 34, 16);
    memcpy(h + 36, "data", 4); put32(h + 40, data);
    fseek(w->file, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, w->file);
}

int ajb_wav_open(ajb_wav_writer_t *w, const char *path, unsigned channels, unsigned rate)
{
    w->file = fopen(path, "wb");
    w->channels = channels;
    w->rate = rate;
    w->frames = 0;
    if (!w->file) return -1;
    header(w);
    return 0;
}

void ajb_wav_write(ajb_wav_writer_t *w, const int16_t *s, uint32_t frames)
{
    for (uint32_t i = 0; i < frames * w->channels; ++i) {
        uint8_t b[2];
        put16(b, (uint16_t)s[i]);
        fwrite(b, 1, 2, w->file);
    }
    w->frames += frames;
}

unsigned ajb_wav_write_q23(ajb_wav_writer_t *w, const int32_t *s, uint32_t frames)
{
    unsigned clipped = 0;
    int16_t buffer[512];
    for (uint32_t done = 0; done < frames;) {
        uint32_t n = frames - done;
        if (n * w->channels > 512U) n = 512U / w->channels;
        for (uint32_t i = 0; i < n * w->channels; ++i) {
            int32_t v = (s[done * w->channels + i] + 128) >> 8;
            if (v > 32767 || v < -32768) { ++clipped; v = v > 0 ? 32767 : -32768; }
            buffer[i] = (int16_t)v;
        }
        ajb_wav_write(w, buffer, n);
        done += n;
    }
    return clipped;
}

int ajb_wav_close(ajb_wav_writer_t *w)
{
    if (!w->file) return -1;
    header(w);
    int r = fclose(w->file);
    w->file = NULL;
    return r;
}

int ajb_wav_read_mono(const char *path, int16_t **pcm, uint32_t *frames, unsigned *rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = size > 12 ? malloc((size_t)size) : NULL;
    int ok = b && fread(b, 1, (size_t)size, f) == (size_t)size && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4);
    fclose(f);
    unsigned channels = 0, bits = 0;
    const uint8_t *data = NULL;
    uint32_t bytes = 0;
    for (long at = 12; ok && at + 8 <= size;) {
        uint32_t length = get32(b + at + 4);
        if (at + 8 + (long)length > size) break;
        if (!memcmp(b + at, "fmt ", 4) && length >= 16) {
            ok = get16(b + at + 8) == 1;
            channels = get16(b + at + 10);
            *rate = get32(b + at + 12);
            bits = get16(b + at + 22);
        } else if (!memcmp(b + at, "data", 4)) {
            data = b + at + 8;
            bytes = length;
        }
        at += 8 + (long)length + (length & 1U);
    }
    if (!ok || !data || bits != 16 || !channels) { free(b); return -1; }
    *frames = bytes / (2U * channels);
    *pcm = malloc(*frames * sizeof **pcm + 1);
    for (uint32_t i = 0; i < *frames; ++i) {
        int32_t sum = 0;
        for (unsigned c = 0; c < channels; ++c) sum += (int16_t)get16(data + 2U * (i * channels + c));
        (*pcm)[i] = (int16_t)(sum / (int32_t)channels);
    }
    free(b);
    return 0;
}
