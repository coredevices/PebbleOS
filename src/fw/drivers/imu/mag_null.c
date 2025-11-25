/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Null Magnetometer Driver

#if CAPABILITY_HAS_MAGNETOMETER
#error This driver is only intended for boards without a magnetometer
#endif

#include "drivers/mag.h"

void mag_use(void) {
}

void mag_start_sampling(void) {
}

void mag_release(void) {
}

MagReadStatus mag_read_data(MagData *data) {
  return MagReadNoMag;
}

bool mag_change_sample_rate(MagSampleRate rate) {
  return false;
}
