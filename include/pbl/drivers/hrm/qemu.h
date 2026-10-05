/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/hrm.h>

/**
 * @defgroup drivers_hrm_qemu QEMU HRM
 * @ingroup drivers_hrm
 * @brief @ref drivers_hrm implementation for QEMU, which emulates no heart rate sensor.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
typedef struct HRMDeviceState {
  bool enabled;
} HRMDeviceState;
/** @endcond */

/** @brief QEMU HRM device. */
typedef const struct HRMDevice {
  /** Driver runtime state. */
  HRMDeviceState *state;
} HRMDevice;

/** @} */
