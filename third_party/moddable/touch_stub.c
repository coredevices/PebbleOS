/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

// Upstream moddable's pebble-touch.c declares but does not define
// touch_sensor_set_enabled() in its !CONFIG_TOUCH branch.
void touch_sensor_set_enabled(bool enabled) {}
