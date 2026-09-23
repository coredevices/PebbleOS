/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"

#include "kernel/pebble_tasks.h"
#include "pbl/services/mic_capture/mic_capture_service.h"

// Builds without the capture service (e.g. PRF) refuse every request.

DEFINE_SYSCALL(uint8_t, sys_mic_capture_start, uint16_t samples_per_update) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return (uint8_t)mic_capture_service_start(pebble_task_get_current(), samples_per_update);
#else
  return (uint8_t)MicCaptureStartErrDenied;
#endif
}

DEFINE_SYSCALL(uint8_t, sys_mic_capture_start_stream, void) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return (uint8_t)mic_capture_service_start_stream(pebble_task_get_current());
#else
  return (uint8_t)MicCaptureStartErrDenied;
#endif
}

DEFINE_SYSCALL(void, sys_mic_capture_stop, void) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  mic_capture_service_stop(pebble_task_get_current());
#endif
}

DEFINE_SYSCALL(uint32_t, sys_mic_capture_read, int16_t *out, uint32_t max_samples) {
  if (PRIVILEGE_WAS_ELEVATED) {
    if (max_samples > MIC_CAPTURE_MAX_SAMPLES_PER_UPDATE) {
      syscall_failed();
    }
    syscall_assert_userspace_buffer(out, max_samples * sizeof(int16_t));
  }
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_read(pebble_task_get_current(), out, max_samples);
#else
  return 0;
#endif
}

DEFINE_SYSCALL(uint32_t, sys_mic_capture_get_available, void) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_get_available();
#else
  return 0;
#endif
}

DEFINE_SYSCALL(bool, sys_mic_capture_is_active, void) {
#ifdef CONFIG_SERVICE_MIC_CAPTURE
  return mic_capture_service_is_active();
#else
  return false;
#endif
}
