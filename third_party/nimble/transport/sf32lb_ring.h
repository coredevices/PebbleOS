/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/util/bits.h>

// Byte ring the LCPU ROM shares with the HCPU. Indexes: offset in the upper half, a mirror flag
// toggled on each wrap in the lower half (same offset: empty if mirrors match, full otherwise).
struct ring {
  uint32_t rd_buf;
  uint32_t wr_buf;
  uint32_t rd_idx;
  uint32_t wr_idx;
  int16_t size;
};

static_assert(sizeof(struct ring) == 20U);

#define RING_IDX_MASK    PBL_GENMASK(31, 16)
#define RING_MIRROR_MASK PBL_GENMASK(15, 0)

static inline uint32_t nimble_hci__ring_used(uint32_t rd, uint32_t wr, uint32_t size) {
  uint32_t rd_idx = PBL_FIELD_GET(RING_IDX_MASK, rd);
  uint32_t wr_idx = PBL_FIELD_GET(RING_IDX_MASK, wr);

  if (rd_idx == wr_idx) {
    return ((rd ^ wr) & RING_MIRROR_MASK) == 0U ? 0U : size;
  }

  return (wr_idx > rd_idx) ? (wr_idx - rd_idx) : (size - (rd_idx - wr_idx));
}

static inline uint32_t nimble_hci__ring_advance(uint32_t ptr, uint32_t len, uint32_t size) {
  uint32_t idx = PBL_FIELD_GET(RING_IDX_MASK, ptr) + len;
  uint32_t mirror = PBL_FIELD_GET(RING_MIRROR_MASK, ptr);

  if (idx >= size) {
    idx -= size;
    mirror ^= RING_MIRROR_MASK;
  }

  return PBL_FIELD_PREP(RING_IDX_MASK, idx) | mirror;
}
