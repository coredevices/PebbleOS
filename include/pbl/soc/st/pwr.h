/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup soc_st_pwr STM32H7 wakeup pins
 * @ingroup soc_st
 * @brief Wakeup pins of the STM32H7 PWR, which wake the SoC from power-off.
 * @{
 */

/**
 * @brief Let a GPIO line on a wakeup pin wake the SoC from power-off.
 *
 * At power-off, the pin wakes on the edge away from the level it is at, with the pull of its line.
 *
 * @param port Registers of the GPIO port.
 * @param pin Pin of the port.
 * @param on Whether the line wakes the SoC.
 *
 * @retval 0 On success.
 * @retval -ENOTSUP The line is not on a wakeup pin.
 */
int pbl_soc_stm32h7_wakeup_pin_set(uintptr_t port, uint32_t pin, bool on);

/** @} */
