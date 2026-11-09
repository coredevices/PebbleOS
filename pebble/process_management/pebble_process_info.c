/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_process_info.h"

#include "pebble_process_md.h"

#include <stddef.h>

// The high bytes sit where the 0x10.0x00 header ended, which the build ID note's alignment
// already left as padding, so the note and everything after it stay where they were.
static_assert(offsetof(PebbleProcessInfo, load_size_hi) == PROCESS_INFO_CRC_START_OFFSET,
              "load_size_hi must start where the 0x10.0x00 header ended");
static_assert(sizeof(PebbleProcessInfo) % 4 == 0,
              "the header must end on the word boundary the build ID note starts at");

int version_compare(Version a, Version b) {
  if (a.major != b.major) {
    return a.major - b.major;
  }
  return a.minor - b.minor;
}

static bool prv_has_wide_sizes(const PebbleProcessInfo *info) {
  const Version first_wide = {
    .major = PROCESS_INFO_FIRST_WIDE_SIZE_STRUCT_VERSION_MAJOR,
    .minor = PROCESS_INFO_FIRST_WIDE_SIZE_STRUCT_VERSION_MINOR,
  };
  return version_compare(info->struct_version, first_wide) >= 0;
}

uint32_t process_info_get_load_size(const PebbleProcessInfo *info) {
  uint32_t size = info->load_size;
  if (prv_has_wide_sizes(info)) {
    size |= (uint32_t)info->load_size_hi << 16;
  }
  return size;
}

uint32_t process_info_get_virtual_size(const PebbleProcessInfo *info) {
  uint32_t size = info->virtual_size;
  if (prv_has_wide_sizes(info)) {
    size |= (uint32_t)info->virtual_size_hi << 16;
  }
  return size;
}
