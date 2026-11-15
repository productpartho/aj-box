/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 * AJ Box's saving: see ajstore.h. */
#include "ajstore.h"
#include "fm1.h"
#include <string.h>

/* AJ Box's records sit in the first sectors of the user partition, below the
 * 456 KiB sample bank (which starts at 69,632): sectors 4 to 11, two for each
 * record. The stock app keeps project slots here; AJ Box has no projects. */
enum { FIRST_SECTOR = 4, MAGIC = 0x58424a41U /* "AJBX" */, VERSION = 1, HEADER_BYTES = 24 };

typedef struct { uint32_t magic, version, record, sequence, bytes, crc; } header_t;
_Static_assert(sizeof(header_t) == HEADER_BYTES, "record header");

static uint32_t crc32(uint32_t crc, const uint8_t *p, unsigned n)
{
    while (n--) {
        crc ^= *p++;
        for (unsigned k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc;
}
static uint32_t record_crc(const header_t *h, const uint8_t *data)
{
    uint32_t crc = crc32(~0U, (const uint8_t *)h, HEADER_BYTES - 4);   /* everything but the crc itself */
    return ~crc32(crc, data, h->bytes);
}
static uint32_t offset_of(unsigned record, unsigned copy)
{
    return (FIRST_SECTOR + 2U * record + copy) * FM1_FLASH_SECTOR_BYTES;
}
/* A copy's header if the copy is whole and its checksum holds; sequence 0 otherwise. */
static uint32_t check(unsigned record, unsigned copy, unsigned bytes, uint8_t *data)
{
    static uint8_t scratch[AJSTORE_MAX_BYTES];
    header_t h;
    if (fm1_flash_read(offset_of(record, copy), &h, sizeof h) != FM1_FLASH_OK) return 0;
    if (h.magic != MAGIC || h.version != VERSION || h.record != record || h.bytes != bytes || !h.sequence) return 0;
    if (!data) data = scratch;
    if (fm1_flash_read(offset_of(record, copy) + HEADER_BYTES, data, bytes) != FM1_FLASH_OK) return 0;
    return record_crc(&h, data) == h.crc ? h.sequence : 0;
}

int ajstore_load(unsigned record, void *data, unsigned bytes)
{
    if (record >= AJSTORE_RECORDS || !data || !bytes || bytes > AJSTORE_MAX_BYTES) return -1;
    if (fm1_flash_size() < offset_of(AJSTORE_RECORDS, 0)) return -2;       /* the SDK reports no usable flash */
    uint32_t a = check(record, 0, bytes, NULL), b = check(record, 1, bytes, NULL);
    if (!a && !b) return -3;
    return check(record, b > a ? 1 : 0, bytes, data) ? 0 : -3;
}

int ajstore_save(unsigned record, const void *data, unsigned bytes)
{
    static uint8_t back[AJSTORE_MAX_BYTES];
    if (record >= AJSTORE_RECORDS || !data || !bytes || bytes > AJSTORE_MAX_BYTES) return -1;
    if (fm1_flash_size() < offset_of(AJSTORE_RECORDS, 0)) return -2;
    /* Write over the older copy, so a save cut off by switch-off leaves the newer one readable. */
    uint32_t a = check(record, 0, bytes, NULL), b = check(record, 1, bytes, NULL);
    unsigned copy = a && (!b || b > a) ? 1 : 0;
    header_t h = {MAGIC, VERSION, record, (a > b ? a : b) + 1U, bytes, 0};
    h.crc = record_crc(&h, data);
    uint32_t at = offset_of(record, copy);
    if (fm1_flash_erase(at) != FM1_FLASH_OK) return -4;
    if (fm1_flash_write(at + HEADER_BYTES, data, bytes) != FM1_FLASH_OK) return -5;
    if (fm1_flash_write(at, &h, sizeof h) != FM1_FLASH_OK) return -5;      /* the header last: it is the commit mark */
    /* Read back and compare byte for byte before calling it saved. */
    header_t read;
    if (fm1_flash_read(at, &read, sizeof read) != FM1_FLASH_OK || memcmp(&read, &h, sizeof h)) return -6;
    if (fm1_flash_read(at + HEADER_BYTES, back, bytes) != FM1_FLASH_OK || memcmp(back, data, bytes)) return -6;
    return 0;
}
