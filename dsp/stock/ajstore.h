/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Copyright (c) 2026 Partho Ghosh.
 *
 * AJ Box's saving (build plan "Saving and update-mode rules"): each thing
 * saved is a record with a version and a checksum, kept in two flash sectors
 * written in turn, read back and compared before it counts as saved. Only
 * the SDK's fm1_flash_size, read, write and erase are used; nothing here
 * looks at the flash chip. Control side only. */
#ifndef AJSTORE_H
#define AJSTORE_H
#include <stdint.h>

/* What is saved. Each record has its own pair of sectors. */
enum { AJSTORE_HEARTS, AJSTORE_JAM, AJSTORE_MEMORY, AJSTORE_PARENT, AJSTORE_RECORDS };
enum { AJSTORE_MAX_BYTES = 4096 - 24 };     /* one 4 KiB sector less the header */

/* 0 when the record is in flash and read back the same; negative when it is
 * not saved (the caller must show it as not saved). */
int ajstore_save(unsigned record, const void *data, unsigned bytes);
/* 0 and `bytes` of data when a good copy exists (the newer of the two);
 * negative when there is none. A bad or half-written copy is ignored. */
int ajstore_load(unsigned record, void *data, unsigned bytes);
#endif
