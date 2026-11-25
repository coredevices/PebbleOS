/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup soc_st_exti STM32H7 EXTI
 * @ingroup soc_st
 * @brief Wakeup lines of the STM32H7 EXTI.
 *
 * Each core masks every line at boot: a line left unmasked for a core that does not take its
 * interrupt keeps the system out of Stop. A driver unmasks the line of its interrupt, for the core
 * it runs on.
 * @{
 */

/**
 * @brief Let a line wake the core running the code from Stop.
 *
 * @param line EXTI line, as the interrupt specifier of the EXTI gives it.
 */
void pbl_soc_stm32h7_wakeup_enable(uint32_t line);

/** @} */
