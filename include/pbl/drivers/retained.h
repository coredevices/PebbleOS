/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup drivers_retained Retained registers
 * @ingroup drivers
 * @brief 32-bit registers that survive a reset.
 *
 * The registers are numbered from 0. Their storage depends on the SoC, such as RTC backup
 * registers or retained RAM, and so does their number. The driver gives them no meaning: each
 * user owns the registers it was configured with, which may also be read by the bootloader.
 * Accessing a register past the last one asserts.
 *
 * @code{.c}
 * pbl_retained_write(CONFIG_FOO_RETAINED_REG, value);
 * @endcode
 * @{
 */

/**
 * @brief Read a retained register.
 *
 * @param reg Register number.
 * @return Register value.
 */
uint32_t pbl_retained_read(uint32_t reg);

/**
 * @brief Write a retained register.
 *
 * @param reg Register number.
 * @param value Value.
 */
void pbl_retained_write(uint32_t reg, uint32_t value);

/** @} */
