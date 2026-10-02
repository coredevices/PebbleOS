/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/hrm.h>

/**
 * @defgroup drivers_hrm_stub HRM stub
 * @ingroup drivers_hrm
 * @brief @ref drivers_hrm implementation for boards without a heart rate sensor.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct HRMDeviceState {
  bool enabled;
} HRMDeviceState;
/** @endcond */

/** @brief Stub HRM device. */
typedef const struct HRMDevice {
  /** Driver runtime state. */
  HRMDeviceState *state;
} HRMDevice;

/** @} */
