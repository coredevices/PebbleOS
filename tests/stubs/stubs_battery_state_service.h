/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/battery_state_service.h"
#include "util/attributes.h"

BatteryChargeState WEAK battery_state_service_peek(void) {
  return (BatteryChargeState) {};
}
