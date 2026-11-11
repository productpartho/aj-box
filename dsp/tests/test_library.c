/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * Factory library checks (PRD "Loop library and storage"): the counts the
 * PRD promises, well-formed patterns, the sample bank budget, every loop's
 * level alone and the limiter on pairs from every page. */
#include "support.h"
#include "sea5.h"
#include <math.h>
#include <string.h>

enum { BANK_BYTES = 466944, LOOPS_MAX = 128 };   /* PRD "Storage math": the stock 456 KiB sample bank */

static int steps_ok(unsigned steps) { return steps == 16 || steps == 32 || steps == 64; }
static int all_in(const char *text, const char *allowed)
{
    return strspn(text, allowed) == strlen(text);
}

static void test_shape(void)
{
    printf("library: 6 pages of 8 + 8 loops, 12 kits, 24 instruments, 8 samples inside the bank\n");
    const ajb_library_t *lib = &ajb_factory_library;
    CHECK(lib->page_count == 6, "%u pages", lib->page_count);
    CHECK(lib->kit_count == 12, "%u kits", lib->kit_count);
    CHECK(lib->instrument_count == 24, "%u instruments", lib->instrument_count);
    CHECK(lib->sample_count == 8, "%u samples", lib->sample_count);
    CHECK(lib->fill && !lib->fill->sliced && lib->fill->steps == 16, "no one-bar Fill loop");

    unsigned sampled = 0, sliced = 0, bytes = 0;
    for (unsigned i = 0; i < lib->instrument_count; ++i) {
        const ajb_instrument_t *in = lib->instruments[i];
        sampled += in->kind == AJB_INSTRUMENT_SAMPLE;
        CHECK(in->name && in->gain_q15, "instrument %u has no name or gain", i);
        CHECK(in->kind != AJB_INSTRUMENT_SAMPLE || (in->sample && !in->sample->slices),
              "%s needs a pitched sample", in->name);
        CHECK(in->kind != AJB_INSTRUMENT_FM || in->program < 128, "%s: voice %u", in->name, in->program);
        CHECK(in->knob_count <= AJB_KNOBS_MAX, "%s: %u knobs", in->name, in->knob_count);
    }
    CHECK(sampled == 4, "%u sampled instruments, PRD says 4", sampled);
    for (unsigned i = 0; i < lib->sample_count; ++i) {
        const ajb_sample_t *s = lib->samples[i];
        sliced += s->slices != 0;
        bytes += ajb_sea5_bytes(s->frames);
        CHECK(s->data && s->frames && s->rate == AJB_SAMPLE_RATE_IN, "%s: no data or not 32 kHz", s->name);
    }
    CHECK(sliced == 4, "%u sliced drum samples, PRD says 4", sliced);
    CHECK(bytes <= BANK_BYTES, "samples take %u bytes of %u", bytes, (unsigned)BANK_BYTES);
    for (unsigned i = 0; i < lib->kit_count; ++i)
        for (unsigned k = 0; k < lib->kits[i]->count; ++k) {
            const ajb_kit_edit_t *edit = &lib->kits[i]->edits[k];
            CHECK(edit->part <= AJB_PARTS && edit->value <= 127, "%s: edit %u out of range", lib->kits[i]->name, k);
        }
    printf("  samples %u bytes = %.1f%% of the bank\n", bytes, 100.0 * bytes / BANK_BYTES);
}

static void test_patterns(void)
{
    printf("library: every pattern is as long as its loop, uses known characters and has a unique name\n");
    const ajb_library_t *lib = &ajb_factory_library;
    const char *names[LOOPS_MAX];
    unsigned count = 0, arps = 0, melodies = 0, used[64] = {0};
    for (unsigned p = 0; p < lib->page_count; ++p) {
        const ajb_page_t *page = &lib->pages[p];
        CHECK(page->kit < lib->kit_count, "page %u: kit %u", p + 1, page->kit);
        for (unsigned k = 0; k < AJB_SLOTS; ++k) {
            const ajb_drum_loop_t *d = page->drums[k];
            const ajb_melody_loop_t *m = page->melody[k];
            CHECK(d && m, "page %u key %u is empty", p + 1, k + 1);
            if (!d || !m) continue;
            if (count + 2 <= LOOPS_MAX) { names[count++] = d->name; names[count++] = m->name; }

            unsigned hits = 0;
            CHECK(steps_ok(d->steps) && d->gain_q15, "%s: %u steps", d->name, d->steps);
            if (d->sliced) {
                hits = d->sliced->slices;
                CHECK(hits && d->steps % hits == 0, "%s: %u slices in %u steps", d->name, hits, d->steps);
            }
            for (unsigned part = 0; !d->sliced && part < AJB_PARTS; ++part) {
                const char *text = d->parts[part];
                if (!text) continue;
                CHECK(strlen(text) == d->steps && all_in(text, ".oxX"), "%s part %u: \"%s\"", d->name, part, text);
                hits += (unsigned)(strlen(text) - strspn(text, "."));
            }
            CHECK(hits, "%s plays nothing", d->name);

            CHECK(steps_ok(m->steps) && m->velocity >= 1 && m->velocity <= 127, "%s: steps or velocity", m->name);
            unsigned known = 0;
            for (unsigned i = 0; i < lib->instrument_count && i < 64; ++i)
                if (lib->instruments[i] == m->instrument) { known = 1; ++used[i]; }
            CHECK(known, "%s: its instrument is not on the ALGORITHM list", m->name);
            ++melodies;
            if (m->riff) {
                CHECK(strlen(m->riff) == m->steps && all_in(m->riff, "0123456789abcdef-."), "%s: \"%s\"",
                      m->name, m->riff);
                CHECK(m->riff[0] != '-' && strpbrk(m->riff, "0123456789abcdef"), "%s: no first note", m->name);
            } else {
                ++arps;
                CHECK(m->chord[0] && m->rate && m->octaves && m->dir <= AJB_ARP_RANDOM, "%s: bad arpeggio", m->name);
                for (unsigned bar = 0; bar < AJB_ARP_BARS; ++bar) {
                    const char *chord = m->chord[bar];
                    CHECK(!chord || (chord[0] && strlen(chord) <= AJB_CHORD_MAX &&
                                     all_in(chord, "0123456789abcdef")), "%s: chord %u", m->name, bar);
                }
            }
        }
    }
    for (unsigned a = 0; a < count; ++a)
        for (unsigned b = a + 1; b < count; ++b)
            CHECK(strcmp(names[a], names[b]), "two loops are called %s", names[a]);
    for (unsigned i = 0; i < lib->instrument_count && i < 64; ++i)
        CHECK(used[i], "no loop uses %s", lib->instruments[i]->name);
    CHECK(melodies && arps * 5 >= melodies * 2 && arps * 5 <= melodies * 3,
          "%u of %u melodies are arpeggios, PRD says about half", arps, melodies);
    printf("  %u loops, %u of %u melodies are arpeggios\n", count, arps, melodies);
}

static void select_loop(ajb_engine_t *e, unsigned side, unsigned page, unsigned slot)
{
    ajb_control(e, (ajb_control_t){AJB_CTL_PAGE, (uint8_t)side, (uint8_t)page});
    ajb_press(e, side, slot);
}
static double q24_db(uint32_t peak) { return peak ? 20.0 * log10(peak / 8388608.0) : -999.0; }

/* Levels where a loop meets the limiter (assets/loops.c): drums near -6
 * dBFS, melodies near -9 dBFS, so no page is louder than another. */
static void test_levels(void)
{
    printf("library: every loop alone is audible and near its level (-6 dBFS drums, -9 dBFS melody)\n");
    const ajb_library_t *lib = &ajb_factory_library;
    double low[AJB_SIDES] = {0, 0}, high[AJB_SIDES] = {-999, -999};
    for (unsigned side = 0; side < AJB_SIDES; ++side)
        for (unsigned p = 0; p < lib->page_count; ++p)
            for (unsigned k = 0; k < AJB_SLOTS; ++k) {
                ajb_engine_t *e = ajb_engine_create(lib);
                select_loop(e, side, p, k);
                render_blocks(e, 4 * 16 * 6014 / AJB_BLOCK);
                double peak = q24_db(ajb_take_side_peak(e, side)), target = side == AJB_DRUMS ? -6.0 : -9.0;
                ajb_engine_destroy(e);
                const char *name = side == AJB_DRUMS ? lib->pages[p].drums[k]->name : lib->pages[p].melody[k]->name;
                CHECK(fabs(peak - target) <= 2.0, "%s peaks at %.2f dBFS, wanted %.0f +/- 2", name, peak, target);
                low[side] = peak < low[side] ? peak : low[side];
                high[side] = peak > high[side] ? peak : high[side];
            }
    printf("  drums %.2f to %.2f dBFS, melodies %.2f to %.2f dBFS\n", low[AJB_DRUMS], high[AJB_DRUMS],
           low[AJB_MELODY], high[AJB_MELODY]);
}

/* The Fill has its own gain: held over a quiet-gain loop (Mirror Ball) and a
 * loud-gain one (Tabla Talk) on the same kit, it comes out at one level. */
static void test_fill_level(void)
{
    printf("library: the Fill is as loud over any loop\n");
    static const uint8_t loops[2] = {1 * AJB_SLOTS + 6, 3 * AJB_SLOTS + 3};
    double peak[2];
    for (unsigned i = 0; i < 2; ++i) {
        ajb_engine_t *e = ajb_engine_create(&ajb_factory_library);
        ajb_control(e, (ajb_control_t){AJB_CTL_PICK, AJB_DRUMS, loops[i]});
        ajb_set_kit(e, 0);
        render_blocks(e, 16 * 6014 / AJB_BLOCK);
        ajb_hold(e, AJB_HOLD_FILL, 1);
        render_blocks(e, 2 * 6014 / AJB_BLOCK);          /* let the loop's last hits ring out */
        ajb_take_side_peak(e, AJB_DRUMS);
        render_blocks(e, 2 * 16 * 6014 / AJB_BLOCK);
        peak[i] = q24_db(ajb_take_side_peak(e, AJB_DRUMS));
        ajb_engine_destroy(e);
    }
    CHECK(fabs(peak[0] - peak[1]) < 0.5, "Fill peaks at %.2f over one loop and %.2f over another", peak[0], peak[1]);
    CHECK(fabs(peak[0] + 6.0) <= 2.0, "Fill peaks at %.2f dBFS, wanted -6 +/- 2", peak[0]);
    printf("  %.2f and %.2f dBFS\n", peak[0], peak[1]);
}

/* Finish: the final chord is several notes at once; it should land near a
 * loop's level, not 10 dB above it, and the next loop is back at full level. */
static void test_finish_level(void)
{
    printf("library: the Finish chord is no louder than a loop, and the next loop is back to normal\n");
    const ajb_library_t *lib = &ajb_factory_library;
    double low = 0, high = -999;
    for (unsigned p = 0; p < lib->page_count; ++p)
        for (unsigned k = 0; k < AJB_SLOTS; k += 3) {
            ajb_engine_t *e = ajb_engine_create(lib);
            select_loop(e, AJB_MELODY, p, k);
            render_blocks(e, 4 * 16 * 6014 / AJB_BLOCK);
            double loop = q24_db(ajb_take_side_peak(e, AJB_MELODY));
            ajb_finish(e);
            render_blocks(e, 16 * 6014 / AJB_BLOCK);            /* up to the bar line; earlier notes die away */
            ajb_take_side_peak(e, AJB_MELODY);
            render_blocks(e, 3 * 16 * 6014 / AJB_BLOCK);
            double chord = q24_db(ajb_take_side_peak(e, AJB_MELODY));
            select_loop(e, AJB_MELODY, p, k);
            render_blocks(e, 4 * 16 * 6014 / AJB_BLOCK);
            double again = q24_db(ajb_take_side_peak(e, AJB_MELODY));
            ajb_engine_destroy(e);
            const char *name = lib->pages[p].melody[k]->name;
            CHECK(chord <= -3.0 && chord >= -20.0, "%s: Finish chord peaks at %.2f dBFS", name, chord);
            CHECK(fabs(again - loop) < 0.75, "%s: %.2f dBFS before Finish, %.2f after", name, loop, again);
            low = chord < low ? chord : low;
            high = chord > high ? chord : high;
        }
    printf("  chords %.2f to %.2f dBFS\n", low, high);
}

/* Each drum loop with two melodies from its own page (the same key and three
 * keys along), 4 bars each, then one pair per page at 4x overdrive. */
static void test_pairs(void)
{
    printf("library: pairs from every page stay under the ceiling\n");
    const ajb_library_t *lib = &ajb_factory_library;
    int32_t worst = 0, worst_stress = 0;
    for (unsigned p = 0; p < lib->page_count; ++p)
        for (unsigned d = 0; d < AJB_SLOTS; ++d)
            for (unsigned shift = 0; shift <= 3; shift += 3) {
                ajb_engine_t *e = ajb_engine_create(lib);
                select_loop(e, AJB_DRUMS, p, d);
                select_loop(e, AJB_MELODY, p, (d + shift) % AJB_SLOTS);
                int32_t peak = render_blocks(e, 4 * 16 * 6014 / AJB_BLOCK);
                worst = peak > worst ? peak : worst;
                ajb_engine_destroy(e);
            }
    for (unsigned p = 0; p < lib->page_count; ++p) {
        ajb_engine_t *e = ajb_engine_create(lib);
        ajb_set_trim(e, 32768U * 4, 32768U * 4);
        select_loop(e, AJB_DRUMS, p, p);
        select_loop(e, AJB_MELODY, p, p);
        int32_t peak = render_blocks(e, 4 * 16 * 6014 / AJB_BLOCK);
        worst_stress = peak > worst_stress ? peak : worst_stress;
        ajb_engine_destroy(e);
    }
    CHECK(worst <= AJB_OUTPUT_CEILING, "a pair peaked at %d (ceiling %d)", worst, AJB_OUTPUT_CEILING);
    CHECK(worst_stress <= AJB_OUTPUT_CEILING, "4x overdrive peaked at %d (ceiling %d)", worst_stress,
          AJB_OUTPUT_CEILING);
    printf("  pair peak %.2f dBFS, overdrive peak %.2f dBFS\n", q24_db((uint32_t)worst),
           q24_db((uint32_t)worst_stress));
}

void test_library(void)
{
    test_shape();
    test_patterns();
    test_levels();
    test_fill_level();
    test_finish_level();
    test_pairs();
}
