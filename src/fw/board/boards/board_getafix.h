/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/led_controller/aw9364e.h"
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
extern I2CBus *const I2C3_BUS;
extern I2CSlavePort *const I2C_LSM2DW12;
extern I2CSlavePort * const I2C_MMC5603NJ;
extern I2CSlavePort * const I2C_NPM1300;
extern I2CSlavePort *const I2C_W1160;
extern I2CSlavePort *const I2C_AW8623X;
extern const Npm1300Config NPM1300_CONFIG;
extern DisplayJDIDevice *const DISPLAY;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const BoardConfigButton BOARD_CONFIG_BUTTON;
extern const MicDevice* MIC;
extern const TouchSensor *CST816;
extern const LedControllerAW9364E AW9364E;

extern const BoardConfigActuator BOARD_CONFIG_VIBE;

static const BoardConfigBacklight BOARD_CONFIG_BACKLIGHT = {
  .options = BacklightOptions_LedController,
};

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .accel_config = {
    // TODO(GETAFIX): Review if correct
    .axes_offsets[AXIS_X] = 0,
    .axes_offsets[AXIS_Y] = 1,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = true,
    .shake_thresholds[AccelThresholdHigh] = 64U,
    .shake_thresholds[AccelThresholdLow] = 15U,
    .double_tap_threshold = 12500U,
    .tap_shock = 0x03U,
    .tap_quiet = 0x02U,
    .tap_dur = 0x08U,
  },
  .accel_ints = {
    [0] = { .peripheral = hwp_gpio1, .gpio_pin = 26 },
  },
};

static const BoardConfigMag BOARD_CONFIG_MAG = {
  // TODO(GETAFIX): Review if correct
  .mag_config = {
    .axes_offsets[AXIS_X] = 1,
    .axes_offsets[AXIS_Y] = 0,
    .axes_offsets[AXIS_Z] = 2,
    .axes_inverts[AXIS_X] = false,
    .axes_inverts[AXIS_Y] = true,
    .axes_inverts[AXIS_Z] = false,
  },
};
