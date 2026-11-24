/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void fake_battery_init(int millivolts, bool usb_connected, bool charging);

void fake_battery_set_millivolts(int millivolts);

//! Make measurements fail with @p error, or succeed again with 0.
void fake_battery_set_measurement_error(int error);

void fake_battery_set_charging(bool charging);

void fake_battery_set_connected(bool usb_connected);
