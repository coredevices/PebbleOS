/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bf0_hal_tim.h"

/**
 * @addtogroup drivers_sf32lb52
 * @{
 */

/**
 * @brief Interrupt handler of the button debounce timer.
 *
 * Connected by the board to the timer set in its button configuration.
 *
 * @param timer Debounce timer.
 */
void debounced_button_irq_handler(GPT_TypeDef *timer);

/** @} */