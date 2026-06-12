/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_GYRO
#error "Use fw/applib/gyro_service.c instead if we have a gyroscope"
#endif

//! @file gyro_service_stub.c
//!
//! Implements the gyro_service for devices that don't actually have a gyroscope
//! (or don't expose one). See fw/applib/gyro_service.c for the real
//! implementation for boards that do.

#include "gyro_service.h"

#include <string.h>

void gyro_data_service_subscribe(uint32_t samples_per_update, GyroDataHandler handler) {
  // Nothing to do: there is no gyroscope, so the handler is never called.
  // Apps should check for the PBL_GYRO define / handle peek failure.
}

void gyro_raw_data_service_subscribe(uint32_t samples_per_update, GyroRawDataHandler handler) {
  // Nothing to do, see above
}

void gyro_data_service_unsubscribe(void) {
  // Nothing to do because we never handle the subscribe in the first place
}

int gyro_service_set_sampling_rate(GyroSamplingRate rate) {
  return -1;
}

int gyro_service_set_samples_per_update(uint32_t num_samples) {
  return -1;
}

int gyro_service_peek(GyroData *data) {
  memset(data, 0, sizeof(*data));
  return -1;
}
