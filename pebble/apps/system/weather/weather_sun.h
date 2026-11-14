/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Sun position for a location today. The synced weather data carries no sun
// times, so they are computed from lat/lon (degrees * 100, INT16_MIN unknown).
// utc_off_min is the location's UTC offset (INT16_MIN: use the watch's own).

//! Local sunset in minutes from midnight [0,1440), or -1 if unknown.
int weather_sun_sunset_minutes(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min);

//! Local start of the evening golden hour (the sun 6 degrees up) in minutes from midnight
//! [0,1440), or -1 if unknown or the sun does not cross that elevation today.
int weather_sun_golden_minutes(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min);

//! Whether the sun is below the horizon at the location now; false if unknown.
bool weather_sun_is_down(int16_t lat_e2, int16_t lon_e2, int16_t utc_off_min);
