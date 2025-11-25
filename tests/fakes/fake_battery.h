/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void fake_battery_init(int millivolts, bool usb_connected, bool charging);

void fake_battery_set_millivolts(int millivolts);

void fake_battery_set_charging(bool charging);

void fake_battery_set_connected(bool usb_connected);
