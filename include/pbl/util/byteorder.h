/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/kernel/compiler.h"

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "only little-endian CPUs are supported");

//! Big-endian 16-bit value, wrapped so it cannot be used without conversion.
typedef struct {
  uint16_t v;
} pbl_be16_t;

static inline uint16_t pbl_be16_to_cpu(uint16_t v) {
  return PBL_BSWAP16(v);
}

static inline uint16_t pbl_cpu_to_be16(uint16_t v) {
  return PBL_BSWAP16(v);
}

static inline uint32_t pbl_be32_to_cpu(uint32_t v) {
  return PBL_BSWAP32(v);
}

static inline uint32_t pbl_cpu_to_be32(uint32_t v) {
  return PBL_BSWAP32(v);
}

static inline uint16_t pbl_le16_to_cpu(uint16_t v) {
  return v;
}

static inline uint16_t pbl_cpu_to_le16(uint16_t v) {
  return v;
}

static inline uint32_t pbl_le32_to_cpu(uint32_t v) {
  return v;
}

static inline uint32_t pbl_cpu_to_le32(uint32_t v) {
  return v;
}

static inline uint16_t pbl_be16_get(pbl_be16_t be) {
  return pbl_be16_to_cpu(be.v);
}

static inline pbl_be16_t pbl_be16_make(uint16_t v) {
  return (pbl_be16_t){pbl_cpu_to_be16(v)};
}
