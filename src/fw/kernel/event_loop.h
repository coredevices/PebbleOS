/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

#include <stdbool.h>

//! Adds an event to the launcher's queue that will call the callback with
//! arbitrary data as argument. Make sure that data points to memory that lives
//! past the point of calling this function.
//! @param callback Function pointer to the callback to be called
//! @param data Pointer to arbitrary data that will be passed as an argument to the callback
void launcher_task_add_callback(CallbackEventCallback callback, void *data);

bool launcher_task_is_current_task(void);

//! Increment or decrement a reference count of services that want the launcher
//! to block pop-ups; used by getting started and firmware update
void launcher_block_popups(bool ignore);

//! Returns true if popups are currently being blocked
bool launcher_popups_are_blocked(void);

void launcher_main_loop(void);

//! Cancel the force quit timer that may currently be running if the back button
//! was pressed down.
void launcher_cancel_force_quit(void);
