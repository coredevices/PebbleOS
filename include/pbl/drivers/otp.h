/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_otp OTP
 * @ingroup drivers
 * @brief One-time programmable slots holding factory data.
 *
 * Each slot holds a NUL-terminated string of up to 31 characters, stored in a flash security
 * register. A slot can be written once.
 *
 * @code{.c}
 * char *serial = otp_get_slot(OTP_SERIAL);
 * @endcode
 * @{
 */

#if defined(CONFIG_BOARD_ASTERIX) || defined(CONFIG_BOARD_OBELIX) ||         \
    defined(CONFIG_BOARD_GETAFIX) || defined(CONFIG_BOARD_QEMU_EMERY) ||     \
    defined(CONFIG_BOARD_QEMU_FLINT) || defined(CONFIG_BOARD_QEMU_GABBRO) || \
    defined(CONFIG_BOARD_NATIVE_EMERY)
/** @brief OTP slot indexes. */
enum {
  /** Hardware version. */
  OTP_HWVER = 0,
  /** Watch serial number. */
  OTP_SERIAL = 1,
  /** PCBA serial number. */
  OTP_PCBA_SERIAL = 2,
  /** Number of slots. */
  NUM_OTP_SLOTS = 3,
};
#else
#error "OTP Slots not set for platform"
#endif

/** @brief Result of otp_write_slot(). */
typedef enum {
  /** Slot written and verified. */
  OtpWriteSuccess = 0,
  /** Slot already holds data. */
  OtpWriteFailAlreadyWritten = 1,
  /** Invalid slot or value, or the write could not be verified. */
  OtpWriteFailCorrupt = 2,
} OtpWriteResult;

/**
 * @brief Get the lock byte of a slot.
 *
 * @param index Slot index.
 * @return Lock byte, or NULL when slots have no individual lock.
 */
uint8_t *otp_get_lock(const uint8_t index);

/**
 * @brief Check whether the OTP storage is locked.
 *
 * Locking applies to the whole security register holding the slots.
 *
 * @param index Slot index.
 * @return true if locked.
 */
bool otp_is_locked(const uint8_t index);

/**
 * @brief Read a slot.
 *
 * @param index Slot index.
 * @return Slot contents, in a static buffer overwritten by the next read, or NULL on error. An
 *         unwritten slot reads as 0xFF bytes.
 */
char *otp_get_slot(const uint8_t index);

/**
 * @brief Write a string to an unwritten slot.
 *
 * @param index Slot index.
 * @param value NUL-terminated string, at most 31 characters.
 * @return Write result.
 */
OtpWriteResult otp_write_slot(const uint8_t index, const char *value);

/** @} */
