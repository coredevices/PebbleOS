/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/util/uuid.h>

#include <kernel/pebble_tasks.h>

//! Whether the process running on [task] may read health data. Always true for other tasks.
bool health_permission_allowed(PebbleTask task);

//! A null uuid means every app's permissions changed.
void health_permission_handle_changed(const Uuid *uuid);
