/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/events.h"
#include "util/attributes.h"

void WEAK event_put(PebbleEvent* event) { }

void WEAK event_put_from_app(PebbleEvent* event) { }

void WEAK event_put_from_process(PebbleTask task, PebbleEvent* event) { }

void WEAK event_reset_from_process_queue(PebbleTask task) { }
