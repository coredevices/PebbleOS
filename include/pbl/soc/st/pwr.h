/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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
 * At power-off, the pin wakes, with the pull of its line, on the edge to the level of
 * @ref PBL_IRQ_TYPE_LEVEL_HIGH or @ref PBL_IRQ_TYPE_LEVEL_LOW, or with
 * @ref PBL_IRQ_TYPE_EDGE_BOTH on the edge away from the level it is at.
 *
 * @param port Registers of the GPIO port.
 * @param pin Pin of the port.
 * @param type Wake type, or 0 for the line not to wake the SoC.
 *
 * @retval 0 On success.
 * @retval -ENOTSUP The line is not on a wakeup pin.
 */
int pbl_soc_stm32h7_wakeup_pin_set(uintptr_t port, uint32_t pin, uint32_t type);

/** @} */
