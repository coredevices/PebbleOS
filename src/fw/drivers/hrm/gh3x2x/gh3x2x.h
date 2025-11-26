/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/i2c_definitions.h"

#if PLATFORM_OBELIX
// FIXME(OBELIX): Provide proper GPIO layer abstraction
#define GH3X2X_RESET_PIN_CTRLBY_NPM1300  1
#endif

typedef struct HRMDeviceState {
  bool enabled;
  bool initialized;
} HRMDeviceState;

typedef const struct HRMDevice {
  HRMDeviceState *state;
  I2CSlavePort *i2c;
  ExtiConfig int_exti;
  OutputConfig reset_gpio;
} HRMDevice;

