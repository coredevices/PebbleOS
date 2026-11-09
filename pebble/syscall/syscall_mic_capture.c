/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/services/mic_capture.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>

PBL_SYSCALL(int32_t, sys_mic_capture_start) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_start(pebble_task_get_current());
#else
  return -MicCaptureStartErrUnavailable;
#endif
}

PBL_SYSCALL(uint32_t, sys_mic_capture_read, (VAL, uint32_t, session),
            (OUT_ARRAY, int16_t *, samples, MIC_CAPTURE_FRAME_SAMPLES)) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_read(pebble_task_get_current(), session, samples);
#else
  return 0;
#endif
}

PBL_SYSCALL(void, sys_mic_capture_stop, (VAL, uint32_t, session)) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  mic_capture_service_stop(pebble_task_get_current(), session);
#endif
}

PBL_SYSCALL(bool, sys_mic_capture_is_active, (VAL, uint32_t, session)) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_is_active(pebble_task_get_current(), session);
#else
  return false;
#endif
}
