/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

#include <stdbool.h>
#include <stdint.h>

//! @internal
//! Structure containing 3-axis magnetometer data
typedef struct PACKED {
 //! magnetic field along the x axis
 int16_t x;
 //! magnetic field along the y axis
 int16_t y;
 //! magnetic field along the z axis
 int16_t z;
} MagData;

typedef enum {
  MagReadSuccess = 0,
  MagReadClobbered = -1,
  MagReadCommunicationFail = -2,
  MagReadMagOff = -3,
  MagReadNoMag = -4,
} MagReadStatus;

typedef enum {
  MagSampleRate20Hz,
  MagSampleRate5Hz
} MagSampleRate;

//! Enable the mag hardware and increment the refcount. Must be matched with a call to
//! mag_release.
void mag_use(void);

//! Enable the mag hardware, configure it in sampling mode and increment the refcount. Must be
//! matched with a call to mag_release.
void mag_start_sampling(void);

//! Release the mag hardware and decrement the refcount. This will turn off the hardware if no one
//! else is still using it (the refcount is still non-zero).
void mag_release(void);

MagReadStatus mag_read_data(MagData *data);

bool mag_change_sample_rate(MagSampleRate rate);

