/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "drivers/led_controller/pwm.h"
#include "drivers/pmic/npm1300.h"
#include "drivers/touch/cst816/touch_sensor_definitions.h"
#include "services/imu/units.h"

#define BT_VENDOR_ID 0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

extern UARTDevice * const DBG_UART;
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern UARTDevice * const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern QSPIPort * const QSPI;
extern QSPIFlash * const QSPI_FLASH;
extern I2CBus *const I2C1_BUS;
extern I2CBus *const I2C2_BUS;
extern I2CSlavePort * const I2C_LSM6D;
extern I2CSlavePort * const I2C_MMC5603NJ;
extern I2CSlavePort * const I2C_NPM1300;
extern I2CSlavePort *const I2C_AW86225;
extern I2CSlavePort *const I2C_W1160;
extern I2CSlavePort *const I2C_AW2016;
extern const Npm1300Config NPM1300_CONFIG;
extern const BoardConfigActuator BOARD_CONFIG_VIBE;
#if !BOARD_OBELIX_BB2
extern const LedControllerPwm LED_CONTROLLER_PWM;
#endif
extern PwmConfig *const PWM1_CH1;
extern DisplayJDIDevice *const DISPLAY;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const BoardConfigButton BOARD_CONFIG_BUTTON;
extern const MicDevice* MIC;
extern HRMDevice * const HRM;
extern const TouchSensor *CST816;
extern const AudioDevice* AUDIO;

static const BoardConfigActuator BOARD_CONFIG_BACKLIGHT = {
  .options = ActuatorOptions_IssiI2C,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
#ifdef IS_BIGBOARD
    .axes_offsets[AXIS_X] = 0,
    .axes_offsets[AXIS_Y] = 1,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = true,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
#else
    .axes_offsets[AXIS_X] = 0,
    .axes_offsets[AXIS_Y] = 1,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = true,
#endif
    // TODO(OBELIX): Needs calibration
    .shake_thresholds[AccelThresholdHigh] = 64U,
    .shake_thresholds[AccelThresholdLow] = 15U,
    .double_tap_threshold = 12500U,
    // LSM6DSO tap timing register values tuned for reliable double-tap:
    // tap_shock (0-3): maximum duration (in ODR steps) where an over-threshold event is still
    //   considered a tap. Higher tolerates longer impacts. 3 = ~max;
    // tap_quiet (0-3): quiet time after first tap during which accel must stay below threshold
    //   before second tap; balances rejection of long impacts vs responsiveness. 2 is moderate.
    // tap_dur (0-15): maximum interval (in ODR steps) between first and second tap. 8 chosen to
    //   allow natural user double taps without allowing widely spaced taps.
    .tap_shock = 0x03U,
    .tap_quiet = 0x02U,
    .tap_dur = 0x08U,
  },
  .accel_int_gpios = {
    [0] = { .gpio = hwp_gpio1, .gpio_pin = 38 },
  },
  .accel_ints = {
    [0] = { .peripheral = hwp_gpio1, .gpio_pin = 38 },
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  .mag_config = {
#ifdef IS_BIGBOARD
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = true,
    .axes_inverts[AXIS_Y] = false,
    .axes_inverts[AXIS_Z] = false,
#else
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
#endif
  },
};
