/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/animation_service.h>

PBL_WEAK void animation_service_cleanup(PebbleTask task) {
}

PBL_WEAK void animation_service_timer_event_received(void) {
}

PBL_WEAK void animation_service_timer_schedule(uint32_t ms) {
}

PBL_WEAK void animation_service_pause(void) {
}

PBL_WEAK void animation_service_resume(void) {
}
