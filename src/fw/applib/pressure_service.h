/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "drivers/pressure.h"

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup PressureService
//!
//! \brief Using the Pebble barometric pressure sensor
//!
//! The PressureService provides access to the barometric pressure sensor for
//! altitude tracking. Apps can subscribe to periodic pressure/altitude readings,
//! configure the data rate, set a reference altitude, and choose between a fast
//! linear approximation or the full barometric formula for altitude calculation.
//!
//! Typical usage for a skydiving altimeter:
//! \code
//! // On the ground, before jump:
//! pressure_service_subscribe(my_handler, PRESSURE_ODR_1HZ);
//! pressure_service_set_reference();     // zero altitude here
//! pressure_service_use_full_formula(true);
//!
//! // When altitude > 30m, switch to high rate:
//! pressure_service_set_data_rate(PRESSURE_ODR_25HZ);
//!
//! // On landing, drop back:
//! pressure_service_set_data_rate(PRESSURE_ODR_1HZ);
//! \endcode
//!     @{

//! Pressure/altitude data delivered to the app handler
typedef struct {
  int32_t pressure_pa;          //!< Absolute pressure in Pascals
  int32_t temperature_centideg; //!< Temperature in 0.01 degrees C
  int32_t altitude_cm;          //!< Altitude in centimeters relative to reference
} PressureData;

//! Callback type for pressure data events
//! @param data Pointer to the latest pressure reading
typedef void (*PressureDataHandler)(PressureData *data);

//! Subscribe to the pressure data service. Once subscribed, the handler is
//! called at the rate specified by \p odr.
//! @param handler Callback to receive pressure data
//! @param odr The desired output data rate
void pressure_service_subscribe(PressureDataHandler handler, PressureODR odr);

//! Unsubscribe from the pressure data service.
void pressure_service_unsubscribe(void);

//! Change the data rate while subscribed.
//! This reconfigures the sensor hardware for the new rate.
//! @param odr The desired output data rate
//! @return true if the rate was changed successfully
bool pressure_service_set_data_rate(PressureODR odr);

//! Capture the current pressure as the reference (altitude = 0).
//! Subsequent altitude readings will be relative to this reference.
//! If no reference is set, altitude is computed relative to standard
//! sea-level pressure (101325 Pa).
//! @return true if the reference was captured successfully
bool pressure_service_set_reference(void);

//! Set the reference pressure to an explicit value.
//! @param ref_pressure_pa Reference pressure in Pascals
void pressure_service_set_reference_pressure(int32_t ref_pressure_pa);

//! Enable or disable the full barometric formula for altitude calculation.
//! When disabled, a faster linear approximation is used (accurate within ~1000m).
//! When enabled, a lookup-table-based hypsometric formula is used (accurate to ~7500m).
//! @param enable true to use the full formula, false for linear approximation
void pressure_service_use_full_formula(bool enable);

//! Read the current pressure data without a subscription (one-shot).
//! @param[out] data Pointer to a PressureData struct to fill
//! @return true if the read succeeded
bool pressure_service_peek(PressureData *data);

//! Calculate altitude relative to a given reference pressure.
//! Uses the full hypsometric formula or linear approximation depending on
//! the current formula setting (see \ref pressure_service_use_full_formula).
//! @param pressure_pa    Current pressure in Pascals
//! @param ref_pressure_pa Reference pressure in Pascals (e.g. 101325 for MSL)
//! @return altitude in centimeters relative to the reference pressure
int32_t pressure_service_get_altitude_cm(int32_t pressure_pa, int32_t ref_pressure_pa);

//!     @} // end addtogroup PressureService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
