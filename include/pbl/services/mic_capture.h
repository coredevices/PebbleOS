/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/mic_capture.h>
#include <kernel/pebble_tasks.h>

#define MIC_CAPTURE_FRAME_SAMPLES 320

void mic_capture_service_init(void);
// Positive session token on success; negated MicCaptureStartResult on failure.
int32_t mic_capture_service_start(PebbleTask task);
size_t mic_capture_service_read(PebbleTask task, uint32_t session, int16_t *samples);
bool mic_capture_service_is_active(PebbleTask task, uint32_t session);
void mic_capture_service_stop(PebbleTask task, uint32_t session);
void mic_capture_service_stop_for_task(PebbleTask task);
void mic_capture_service_handle_focus_lost(void);
void mic_capture_service_handle_permission_changed(void);
