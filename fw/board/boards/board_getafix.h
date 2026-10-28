/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/touch/cst816/touch_sensor_definitions.h>
#include <pbl/services/imu/units.h>

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

extern UARTDevice *const DBG_UART;
#ifdef NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern UARTDevice *const HCI_TRACE_UART;
#endif // NIMBLE_HCI_SF32LB52_TRACE_BINARY
extern QSPIPort *const QSPI;
extern QSPIFlash *const QSPI_FLASH;
extern DisplayJDIDevice *const DISPLAY;
extern const BoardConfigPower BOARD_CONFIG_POWER;
extern const BoardConfig BOARD_CONFIG;
extern const MicDevice *MIC;
extern const TouchSensor *CST816;

static const BoardConfigAccel BOARD_CONFIG_ACCEL = {
  .default_motion_sensitivity = 55U, // Medium
};
