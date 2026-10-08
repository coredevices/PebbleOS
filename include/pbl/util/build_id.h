/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_build_id Build ID
 * @ingroup util
 * @brief GNU build ID note inserted by the linker.
 * @{
 */

/** @brief ELF note, the format the linker uses to insert the build ID. */
typedef struct PBL_PACKED {
  /** Length of the name, including its terminator. */
  uint32_t name_length;
  /** Length of the descriptor (the build ID) in bytes. */
  uint32_t data_length;
  /** Note type, 3 (@c NT_GNU_BUILD_ID) for a build ID. */
  uint32_t type;
  /** Name ("GNU" for a build ID) followed by the descriptor. */
  uint8_t data[];
} ElfExternalNote;

/** @brief Length of the build ID, a SHA-1 hash. */
#define BUILD_ID_EXPECTED_LEN (20)

/** @brief Length of the build ID note name, "GNU" and its terminator. */
#define BUILD_ID_NAME_EXPECTED_LEN (4)

/** @brief Total size of a build ID note. */
#define BUILD_ID_TOTAL_EXPECTED_LEN \
  (sizeof(ElfExternalNote) + BUILD_ID_NAME_EXPECTED_LEN + BUILD_ID_EXPECTED_LEN)

/**
 * @brief Check whether a note holds a GNU build ID of the expected length.
 *
 * @param note ELF note.
 * @return true if @p note is a 20-byte @c NT_GNU_BUILD_ID note named "GNU".
 */
bool build_id_contains_gnu_build_id(const ElfExternalNote *note);

/** @} */
