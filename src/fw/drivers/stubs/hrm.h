/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/hrm.h"

typedef struct HRMDeviceState {
  bool enabled;
} HRMDeviceState;

typedef const struct HRMDevice {
  HRMDeviceState *state;
} HRMDevice;
