/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup PressureService
//!
//! \brief Reads the barometric pressure sensor
//!
//! On platforms with a barometer (PBL_BAROMETER), the PressureService measures the air pressure.
//! The sensor sleeps between readings: each call to pressure_service_peek() runs one
//! measurement, which takes a few tens of milliseconds.
//! @{

//! Measure the air pressure.
//! @param[out] pressure_pa Air pressure in pascals (1 hPa = 100 Pa), not corrected to sea level
//! @return true on success, false if the watch has no pressure sensor or the measurement failed
bool pressure_service_peek(uint32_t *pressure_pa);

//!   @} // end addtogroup PressureService
//! @} // end addtogroup Foundation
