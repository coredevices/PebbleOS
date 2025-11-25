/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/hrm.h"

#include "board/board.h"
#include "drivers/i2c_definitions.h"
#include "os/mutex.h"
#include "services/common/new_timer/new_timer.h"
#include "util/attributes.h"

#include <stdbool.h>


typedef enum HRMEnabledState {
  HRMEnabledState_Uninitialized = 0,
  HRMEnabledState_Disabled,
  HRMEnabledState_PoweringOn,
  HRMEnabledState_Enabled,
} HRMEnabledState;

typedef struct HRMDeviceState {
  HRMEnabledState enabled_state;
  PebbleMutex *lock;
  TimerID timer;
  uint32_t handshake_count;
} HRMDeviceState;

typedef const struct HRMDevice {
  HRMDeviceState *state;
  ExtiConfig handshake_int;
  InputConfig int_gpio;
  OutputConfig en_gpio;
  I2CSlavePort *i2c_slave;
} HRMDevice;

typedef struct PACKED AS7000InfoRecord {
  uint8_t protocol_version_major;
  uint8_t protocol_version_minor;
  uint8_t sw_version_major;
  uint8_t sw_version_minor;
  uint8_t application_id;
  uint8_t hw_revision;
} AS7000InfoRecord;

//! Fills a struct which contains version info about the AS7000
//! This should probably only be used by the HRM Demo app
void as7000_get_version_info(HRMDevice *dev, AS7000InfoRecord *info_out);
