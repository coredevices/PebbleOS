/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_retained Retained registers
 * @ingroup drivers
 * @brief 32-bit registers that survive a reset.
 *
 * Each SoC maps the registers onto its own storage, such as RTC backup registers or retained
 * RAM. The bootloader reads some of them, so a register never moves once a SoC maps it. Not
 * every SoC maps every register; accessing an unmapped one asserts.
 *
 * @code{.c}
 * pbl_retained_write(PBL_RETAINED_LAST_APP_SLOT, slot);
 * @endcode
 * @{
 */

/** @brief Retained registers. */
enum pbl_retained_reg {
  /** Boot bits, shared with the bootloader. */
  PBL_RETAINED_BOOTBITS,
  /** Bootloader version, written by the bootloader. */
  PBL_RETAINED_BOOT_VERSION,
  /** Reboot reason, word 0. */
  PBL_RETAINED_REBOOT_REASON_0,
  /** Reboot reason, word 1. */
  PBL_RETAINED_REBOOT_REASON_1,
  /** Reboot reason, word 2. */
  PBL_RETAINED_REBOOT_REASON_2,
  /** Reboot reason, word 3. */
  PBL_RETAINED_REBOOT_REASON_3,
  /** Slot of the last launched app. */
  PBL_RETAINED_LAST_APP_SLOT,
  /** RTC wall time at the last save. */
  PBL_RETAINED_RTC_TIME,
  /** RTC ticks at the last save. */
  PBL_RETAINED_RTC_TICKS,
  /** Timezone, word 0. */
  PBL_RETAINED_TZ_0,
  /** Timezone, word 1. */
  PBL_RETAINED_TZ_1,
  /** Timezone, word 2. */
  PBL_RETAINED_TZ_2,
  /** Timezone, word 3. */
  PBL_RETAINED_TZ_3,
  /** Timezone, word 4. */
  PBL_RETAINED_TZ_4,
  /** Number of registers. */
  PBL_RETAINED_COUNT,
};

/**
 * @brief Read a retained register.
 *
 * @param reg Register.
 * @return Register value.
 */
uint32_t pbl_retained_read(enum pbl_retained_reg reg);

/**
 * @brief Write a retained register.
 *
 * @param reg Register.
 * @param value Value.
 */
void pbl_retained_write(enum pbl_retained_reg reg, uint32_t value);

/** @} */
