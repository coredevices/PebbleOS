/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "bf0_hal_tim.h"

void debounced_button_irq_handler(GPT_TypeDef *timer);