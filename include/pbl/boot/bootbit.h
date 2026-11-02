/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup boot_bootbit Boot bits
 * @ingroup subsys
 * @brief Flags shared with the bootloader across resets.
 *
 * The bootloader reads and writes the same bits, so their positions never change. Bits the
 * firmware does not use are left out.
 *
 * @code{.c}
 * pbl_bootbit_set(PBL_BOOTBIT_FORCE_PRF);
 * system_reset();
 * @endcode
 * @{
 */

/** @brief Boot bits. */
enum pbl_bootbit {
  /** The boot bits hold valid values. */
  PBL_BOOTBIT_INITIALIZED = 0,
  /** A new firmware is ready to be installed by the bootloader. */
  PBL_BOOTBIT_NEW_FW_AVAILABLE = 1,
  /** The bootloader is installing a new firmware. */
  PBL_BOOTBIT_NEW_FW_UPDATE_IN_PROGRESS = 2,
  /** Firmware start failure counter, bit 0. */
  PBL_BOOTBIT_FW_START_FAIL_STRIKE_ONE = 3,
  /** Firmware start failure counter, bit 1. */
  PBL_BOOTBIT_FW_START_FAIL_STRIKE_TWO = 4,
  /** PRF start failure counter, bit 0. */
  PBL_BOOTBIT_RECOVERY_LOAD_FAIL_STRIKE_ONE = 5,
  /** PRF start failure counter, bit 1. */
  PBL_BOOTBIT_RECOVERY_LOAD_FAIL_STRIKE_TWO = 6,
  /** The bootloader is starting PRF. */
  PBL_BOOTBIT_RECOVERY_START_IN_PROGRESS = 7,
  /** The firmware failed and reset. */
  PBL_BOOTBIT_SOFTWARE_FAILURE_OCCURRED = 9,
  /** Reset loop counter, bit 0. */
  PBL_BOOTBIT_RESET_LOOP_DETECT_ONE = 11,
  /** Reset loop counter, bit 1. */
  PBL_BOOTBIT_RESET_LOOP_DETECT_TWO = 12,
  /** Reset loop counter, bit 2. */
  PBL_BOOTBIT_RESET_LOOP_DETECT_THREE = 13,
  /** The firmware has run long enough to be considered stable. */
  PBL_BOOTBIT_FW_STABLE = 14,
  /** The bootloader installed a new firmware. */
  PBL_BOOTBIT_NEW_FW_INSTALLED = 15,
  /** Start PRF on the next boot. */
  PBL_BOOTBIT_FORCE_PRF = 17,
  /** A new PRF is ready, installed by the firmware on the next boot. */
  PBL_BOOTBIT_NEW_PRF_AVAILABLE = 18,
};

/**
 * @brief Set a boot bit.
 *
 * @param bit Boot bit.
 */
void pbl_bootbit_set(enum pbl_bootbit bit);

/**
 * @brief Clear a boot bit.
 *
 * @param bit Boot bit.
 */
void pbl_bootbit_clear(enum pbl_bootbit bit);

/**
 * @brief Test a boot bit.
 *
 * @param bit Boot bit.
 * @return true if the bit is set.
 */
bool pbl_bootbit_test(enum pbl_bootbit bit);

/**
 * @brief Get the firmware start failure counter.
 *
 * @return Counter value, 0 to 3.
 */
uint8_t pbl_bootbit_fw_fail_cnt_get(void);

/**
 * @brief Set the reset loop counter.
 *
 * @param cnt Counter value, 0 to 7.
 */
void pbl_bootbit_reset_loop_cnt_set(uint8_t cnt);

/** @} */
