/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"

void shell_event_loop_init(void);

//! Handle events relating to the base firmware UI
void NOINLINE shell_event_loop_handle_event(PebbleEvent *event);

