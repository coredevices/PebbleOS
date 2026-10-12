/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/mic_capture.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>
#include <syscall/syscall_internal.h>

DEFINE_SYSCALL(int32_t, sys_mic_capture_start, void) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_start(pebble_task_get_current());
#else
  return -MicCaptureStartErrUnavailable;
#endif
}

DEFINE_SYSCALL(uint32_t, sys_mic_capture_read, uint32_t session, int16_t *samples) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(samples, MIC_CAPTURE_FRAME_SAMPLES * sizeof(*samples));
  }
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_read(pebble_task_get_current(), session, samples);
#else
  return 0;
#endif
}

DEFINE_SYSCALL(void, sys_mic_capture_stop, uint32_t session) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  mic_capture_service_stop(pebble_task_get_current(), session);
#endif
}

DEFINE_SYSCALL(bool, sys_mic_capture_is_active, uint32_t session) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_is_active(pebble_task_get_current(), session);
#else
  return false;
#endif
}
