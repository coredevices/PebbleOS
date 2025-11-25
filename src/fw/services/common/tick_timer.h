/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"

void tick_timer_add_subscriber(PebbleTask task);
void tick_timer_remove_subscriber(PebbleTask task);
