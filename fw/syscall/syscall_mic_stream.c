/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/mic_stream.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>
#include <syscall/syscall_internal.h>

DEFINE_SYSCALL(uint8_t, sys_mic_stream_start, void) {
#ifdef CONFIG_SERVICE_MIC_STREAM
  return (uint8_t)mic_stream_service_start(pebble_task_get_current());
#else
  return (uint8_t)MicStreamServiceStartErrUnavailable;
#endif
}

DEFINE_SYSCALL(void, sys_mic_stream_stop, void) {
#ifdef CONFIG_SERVICE_MIC_STREAM
  mic_stream_service_stop(pebble_task_get_current());
#endif
}
