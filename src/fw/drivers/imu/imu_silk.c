/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/accel.h"
#include "drivers/imu.h"
#include "drivers/imu/bma255/bma255.h"

void imu_init(void) {
  bma255_init();
}

void imu_power_up(void) {
  // NYI
}

void imu_power_down(void) {
  // NYI
}

#if !TARGET_QEMU

////////////////////////////////////
// Accel
//
////////////////////////////////////

void accel_enable_double_tap_detection(bool on) {
}

bool accel_get_double_tap_detection_enabled(void) {
    return false;
}

#endif /* !TARGET_QEMU */
