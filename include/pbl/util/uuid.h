/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/list.h>
#include <stdint.h>
#include <stdbool.h>
#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_uuid UUID
 * @ingroup util
 * @brief 128-bit universally unique identifiers.
 *
 * Uuid, its constructors, uuid_equal() and uuid_to_string() are also part of the app SDK.
 *
 * @code{.c}
 * Uuid uuid;
 * char str[UUID_STRING_BUFFER_LENGTH];
 *
 * uuid_generate(&uuid);
 * uuid_to_string(&uuid, str);
 * @endcode
 * @{
 */

/** Size of a UUID, in bytes. */
#define UUID_SIZE 16

/** @brief A 128-bit UUID, with its bytes in the order they are written in string form. */
typedef struct PBL_PACKED {
  /** Byte 0. */
  uint8_t byte0;
  /** Byte 1. */
  uint8_t byte1;
  /** Byte 2. */
  uint8_t byte2;
  /** Byte 3. */
  uint8_t byte3;
  /** Byte 4. */
  uint8_t byte4;
  /** Byte 5. */
  uint8_t byte5;
  /** Byte 6. */
  uint8_t byte6;
  /** Byte 7. */
  uint8_t byte7;
  /** Byte 8. */
  uint8_t byte8;
  /** Byte 9. */
  uint8_t byte9;
  /** Byte 10. */
  uint8_t byte10;
  /** Byte 11. */
  uint8_t byte11;
  /** Byte 12. */
  uint8_t byte12;
  /** Byte 13. */
  uint8_t byte13;
  /** Byte 14. */
  uint8_t byte14;
  /** Byte 15. */
  uint8_t byte15;
} Uuid;

/** @brief Initializer of the all-zero UUID that identifies the system. */
#define UUID_SYSTEM {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
/** @brief Initializer of the all-ones invalid UUID. */
#define UUID_INVALID_INIT \
  {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
/** The invalid UUID, all bytes 0xff. */
#define UUID_INVALID ((const Uuid)UUID_INVALID_INIT)

/**
 * Make a Uuid object from sixteen bytes.
 * @param p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15 Bytes, in string order.
 * @return A Uuid structure representing the bytes p0 to p15.
 */
#define UuidMake(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) \
  ((Uuid){p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15})

/**
 * Creates a Uuid from an array of bytes with 16 bytes in Big Endian order.
 * @param b Array of 16 bytes.
 * @return The created Uuid
 */
#define UuidMakeFromBEBytes(b)                                                                     \
  ((Uuid){                                                                                         \
    b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], \
    b[15]                                                                                          \
  })

/**
 * Creates a Uuid from an array of bytes with 16 bytes in Little Endian order.
 * @param b Array of 16 bytes.
 * @return The created Uuid
 */
#define UuidMakeFromLEBytes(b)                                                                \
  ((Uuid){                                                                                    \
    b[15], b[14], b[13], b[12], b[11], b[10], b[9], b[8], b[7], b[6], b[5], b[4], b[3], b[2], \
    b[1], b[0]                                                                                \
  })

/**
 * @brief Compares two UUIDs.
 * @param uu1 First UUID.
 * @param uu2 Second UUID.
 * @return True if the two UUIDs are equal, false if they are not or either is NULL.
 */
bool uuid_equal(const Uuid *uu1, const Uuid *uu2);

/**
 * @brief Generate a random (version 4) UUID.
 *
 * Uses pbl_rand32().
 *
 * @param[out] uuid_out Generated UUID.
 */
void uuid_generate(Uuid *uuid_out);

/**
 * @brief Check whether a UUID is the system UUID (all zeros).
 *
 * @param uuid UUID to test.
 * @return true if @p uuid equals @ref UUID_SYSTEM, false otherwise or for NULL.
 */
bool uuid_is_system(const Uuid *uuid);

/**
 * @brief Check whether a UUID is invalid.
 *
 * @param uuid UUID to test.
 * @return true if @p uuid is NULL or equals UUID_INVALID.
 */
bool uuid_is_invalid(const Uuid *uuid);

/** The minimum required length of a string used to hold a uuid (including null). */
#define UUID_STRING_BUFFER_LENGTH (32 + 4 + 2 + 1) // numbers + dashes + brackets + trailing null

/**
 * @brief Writes UUID in a string form into buffer that looks like the following...
 * {12345678-1234-5678-1234-567812345678} or {NULL UUID} if NULL was passed.
 * @param uuid The Uuid to write into the buffer as human-readable string
 * @param[out] buffer Memory to write the string to. Must be at least @ref UUID_STRING_BUFFER_LENGTH
 * bytes long.
 */
void uuid_to_string(const Uuid *uuid, char *buffer);

/** @} */
