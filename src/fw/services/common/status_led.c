/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/common/status_led.h"
#include "system/passert.h"
#include "services/common/battery/battery_curve.h"
#include "drivers/led_controller.h"
#include "board/board.h"
#include "system/passert.h"

void status_led_set(StatusLedState state) {
  // No LED present, do nothing!
}
