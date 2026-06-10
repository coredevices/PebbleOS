/* SPDX-FileCopyrightText: 2026 Dave Bortz */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/gyro.h"

#include <stdbool.h>
#include <stdint.h>

//! Gyroscope manager service.
//!
//! Kernel-side owner of the gyroscope driver (drivers/gyro.h): provides the
//! driver's offload and sample callbacks and tracks the most recent sample.
//! Client subscription plumbing (mirroring the accel manager) will be built
//! on top of this.

void gyro_manager_init(void);

//! Peek at the most recent gyroscope sample. Powers nothing up: fails when
//! the gyroscope is not currently running.
//! @return 0 on success, nonzero on failure
int gyro_manager_peek(GyroDriverSample *data);
