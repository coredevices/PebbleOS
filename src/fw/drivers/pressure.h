/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Initialize the pressure sensor driver. Call this once at startup.
void pressure_init(void);

//! Read the current pressure and temperature from the sensor.
//! @param pressure_pa         Output: pressure in Pascals (NULL to skip)
//! @param temperature_centideg Output: temperature in 0.01 degrees C (NULL to skip)
//! @return true if read succeeded, false on error or if sensor not initialized
bool pressure_read(int32_t *pressure_pa, int32_t *temperature_centideg);

//! Calculate altitude from pressure using a linear approximation of the
//! barometric formula. Accurate within ~1000m of the reference pressure altitude.
//! @param pressure_pa    Current pressure in Pascals
//! @param sea_level_pa   Reference sea-level pressure in Pascals (e.g. 101325)
//! @return altitude in centimeters relative to the reference pressure
int32_t pressure_get_altitude_cm(int32_t pressure_pa, int32_t sea_level_pa);

//! Calculate altitude using the full hypsometric barometric formula with a
//! lookup table. Accurate across the full range 0–7500m.
//! @param pressure_pa    Current pressure in Pascals
//! @param ref_pressure_pa Reference pressure in Pascals (e.g. ground-level reading)
//! @return altitude in centimeters relative to the reference pressure
int32_t pressure_get_altitude_full_cm(int32_t pressure_pa, int32_t ref_pressure_pa);

//! BMP390 output data rate presets
typedef enum {
  PRESSURE_ODR_1HZ = 0,    //!< 1 Hz — low power, good for background monitoring
  PRESSURE_ODR_5HZ,        //!< 5 Hz — moderate rate
  PRESSURE_ODR_10HZ,       //!< 10 Hz — responsive
  PRESSURE_ODR_25HZ,       //!< 25 Hz — high rate, suitable for skydiving
  PRESSURE_ODR_50HZ,       //!< 50 Hz — very high rate
} PressureODR;

//! Configure the sensor output data rate and adjust oversampling/filter
//! to match. Higher rates reduce oversampling for faster response.
//! @param odr The desired output data rate
//! @return true if configuration succeeded
bool pressure_set_odr(PressureODR odr);
