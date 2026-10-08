/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <applib/battery_state_service.h>

BatteryChargeState PBL_WEAK battery_state_service_peek(void) {
  return (BatteryChargeState){};
}
