/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include "drivers/pmic.h"
#include "drivers/gpio.h"
#include "util/delay.h"
#include "board/board.h"

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"

void silk_rail_ctl_fn(bool enable) {
  // NYI
  return;
}

void silk_rail_cfg_fn(void) {
  // NYI
  return;
}
