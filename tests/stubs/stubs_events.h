/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <kernel/events.h>

void PBL_WEAK event_put(PebbleEvent *event) {
}

void PBL_WEAK event_put_from_app(PebbleEvent *event) {
}

void PBL_WEAK event_put_from_process(PebbleTask task, PebbleEvent *event) {
}

void PBL_WEAK event_reset_from_process_queue(PebbleTask task) {
}
