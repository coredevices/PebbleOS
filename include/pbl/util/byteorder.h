/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <string.h>

#include <pbl/kernel/compiler.h>

static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "only little-endian CPUs are supported");

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
 *
 * uint32_t addr = pbl_get_le32(&buf[1]);
 * pbl_put_be16(reg, cmd);
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
 * @brief Convert a big-endian 64-bit value to CPU order.
 *
 * @param v Big-endian value.
 * @return Value in CPU order.
 */
static inline uint64_t pbl_be64_to_cpu(uint64_t v) {
  return PBL_BSWAP64(v);
}

/**
 * @brief Convert a 64-bit value in CPU order to big-endian.
 *
 * @param v Value in CPU order.
 * @return Big-endian value.
 */
static inline uint64_t pbl_cpu_to_be64(uint64_t v) {
  return PBL_BSWAP64(v);
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
 * @brief Convert a little-endian 64-bit value to CPU order.
 *
 * @param v Little-endian value.
 * @return Value in CPU order.
 */
static inline uint64_t pbl_le64_to_cpu(uint64_t v) {
  return v;
}

/**
 * @brief Convert a 64-bit value in CPU order to little-endian.
 *
 * @param v Value in CPU order.
 * @return Little-endian value.
 */
static inline uint64_t pbl_cpu_to_le64(uint64_t v) {
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

/**
 * @brief Read a little-endian 16-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 2 bytes.
 * @return Value in CPU order.
 */
static inline uint16_t pbl_get_le16(const void *src) {
  uint16_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_le16_to_cpu(v);
}

/**
 * @brief Write a 16-bit value as little-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 2 bytes.
 */
static inline void pbl_put_le16(uint16_t val, void *dst) {
  const uint16_t v = pbl_cpu_to_le16(val);

  memcpy(dst, &v, sizeof(v));
}

/**
 * @brief Read a big-endian 16-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 2 bytes.
 * @return Value in CPU order.
 */
static inline uint16_t pbl_get_be16(const void *src) {
  uint16_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_be16_to_cpu(v);
}

/**
 * @brief Write a 16-bit value as big-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 2 bytes.
 */
static inline void pbl_put_be16(uint16_t val, void *dst) {
  const uint16_t v = pbl_cpu_to_be16(val);

  memcpy(dst, &v, sizeof(v));
}

/**
 * @brief Read a little-endian 24-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 3 bytes.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_get_le24(const void *src) {
  const uint8_t *p = src;

  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/**
 * @brief Write a 24-bit value as little-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 3 bytes.
 */
static inline void pbl_put_le24(uint32_t val, void *dst) {
  uint8_t *p = dst;

  p[0] = (uint8_t)val;
  p[1] = (uint8_t)(val >> 8);
  p[2] = (uint8_t)(val >> 16);
}

/**
 * @brief Read a big-endian 24-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 3 bytes.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_get_be24(const void *src) {
  const uint8_t *p = src;

  return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

/**
 * @brief Write a 24-bit value as big-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 3 bytes.
 */
static inline void pbl_put_be24(uint32_t val, void *dst) {
  uint8_t *p = dst;

  p[0] = (uint8_t)(val >> 16);
  p[1] = (uint8_t)(val >> 8);
  p[2] = (uint8_t)val;
}

/**
 * @brief Read a little-endian 32-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 4 bytes.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_get_le32(const void *src) {
  uint32_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_le32_to_cpu(v);
}

/**
 * @brief Write a 32-bit value as little-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 4 bytes.
 */
static inline void pbl_put_le32(uint32_t val, void *dst) {
  const uint32_t v = pbl_cpu_to_le32(val);

  memcpy(dst, &v, sizeof(v));
}

/**
 * @brief Read a big-endian 32-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 4 bytes.
 * @return Value in CPU order.
 */
static inline uint32_t pbl_get_be32(const void *src) {
  uint32_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_be32_to_cpu(v);
}

/**
 * @brief Write a 32-bit value as big-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 4 bytes.
 */
static inline void pbl_put_be32(uint32_t val, void *dst) {
  const uint32_t v = pbl_cpu_to_be32(val);

  memcpy(dst, &v, sizeof(v));
}

/**
 * @brief Read a little-endian 64-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 8 bytes.
 * @return Value in CPU order.
 */
static inline uint64_t pbl_get_le64(const void *src) {
  uint64_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_le64_to_cpu(v);
}

/**
 * @brief Write a 64-bit value as little-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 8 bytes.
 */
static inline void pbl_put_le64(uint64_t val, void *dst) {
  const uint64_t v = pbl_cpu_to_le64(val);

  memcpy(dst, &v, sizeof(v));
}

/**
 * @brief Read a big-endian 64-bit value from a possibly unaligned buffer.
 *
 * @param src Buffer holding 8 bytes.
 * @return Value in CPU order.
 */
static inline uint64_t pbl_get_be64(const void *src) {
  uint64_t v;

  memcpy(&v, src, sizeof(v));

  return pbl_be64_to_cpu(v);
}

/**
 * @brief Write a 64-bit value as big-endian to a possibly unaligned buffer.
 *
 * @param val Value in CPU order.
 * @param dst Buffer receiving 8 bytes.
 */
static inline void pbl_put_be64(uint64_t val, void *dst) {
  const uint64_t v = pbl_cpu_to_be64(val);

  memcpy(dst, &v, sizeof(v));
}

/** @} */
