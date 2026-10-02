/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/kernel/compiler.h"

_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "only little-endian CPUs are supported");

/**
 * @defgroup util_byteorder Byte order
 * @ingroup util
 * @brief Conversions between the CPU byte order (always little-endian) and fixed byte orders.
 *
 * @code{.c}
 * struct PBL_PACKED hdr {
 *   pbl_be16_t length;
 * } hdr;
 *
 * hdr.length = pbl_be16_make(payload_len);
 * uint16_t len = pbl_be16_get(hdr.length);
 * @endcode
 * @{
 */

/** @brief Big-endian 16-bit value, wrapped so it cannot be used without conversion. */
typedef struct {
  /** Raw big-endian value. */
  uint16_t v;
} pbl_be16_t;

/**
 * @brief Convert a big-endian 16-bit value to CPU order.
 *
 * @param v Big-endian value.
 * @return Value in CPU order.
 */
static inline uint16_t pbl_be16_to_cpu(uint16_t v) {
  return PBL_BSWAP16(v);
}

/**
 * @brief Convert a 16-bit value in CPU order to big-endian.
 *
 * @param v Value in CPU order.
 * @return Big-endian value.
 */
static inline uint16_t pbl_cpu_to_be16(uint16_t v) {
  return PBL_BSWAP16(v);
}

/**
 * @brief Convert a big-endian 32-bit value to CPU order.
 *
 * @param v Big-endian value.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_be32_to_cpu(uint32_t v) {
  return PBL_BSWAP32(v);
}

/**
 * @brief Convert a 32-bit value in CPU order to big-endian.
 *
 * @param v Value in CPU order.
 * @return Big-endian value.
 */
static inline uint32_t pbl_cpu_to_be32(uint32_t v) {
  return PBL_BSWAP32(v);
}

/**
 * @brief Convert a little-endian 16-bit value to CPU order.
 *
 * @param v Little-endian value.
 * @return Value in CPU order.
 */
static inline uint16_t pbl_le16_to_cpu(uint16_t v) {
  return v;
}

/**
 * @brief Convert a 16-bit value in CPU order to little-endian.
 *
 * @param v Value in CPU order.
 * @return Little-endian value.
 */
static inline uint16_t pbl_cpu_to_le16(uint16_t v) {
  return v;
}

/**
 * @brief Convert a little-endian 32-bit value to CPU order.
 *
 * @param v Little-endian value.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_le32_to_cpu(uint32_t v) {
  return v;
}

/**
 * @brief Convert a 32-bit value in CPU order to little-endian.
 *
 * @param v Value in CPU order.
 * @return Little-endian value.
 */
static inline uint32_t pbl_cpu_to_le32(uint32_t v) {
  return v;
}

/**
 * @brief Read a wrapped big-endian 16-bit value.
 *
 * @param be Big-endian value.
 * @return Value in CPU order.
 */
static inline uint16_t pbl_be16_get(pbl_be16_t be) {
  return pbl_be16_to_cpu(be.v);
}

/**
 * @brief Wrap a 16-bit value as big-endian.
 *
 * @param v Value in CPU order.
 * @return Big-endian value.
 */
static inline pbl_be16_t pbl_be16_make(uint16_t v) {
  return (pbl_be16_t){pbl_cpu_to_be16(v)};
}

/** @} */
