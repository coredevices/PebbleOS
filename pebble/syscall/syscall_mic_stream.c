/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/syscall.h>
#include <pbl/services/mic_stream.h>

#include <kernel/pebble_tasks.h>
#include <syscall/syscall.h>

PBL_SYSCALL(uint8_t, sys_mic_stream_start, (OUT, uint32_t *, stream_id)) {
#ifdef CONFIG_SERVICE_MIC_STREAM
  return (uint8_t)mic_stream_service_start(pebble_task_get_current(), stream_id);
#else
  return (uint8_t)MicStreamServiceStartErrUnavailable;
#endif
}

PBL_SYSCALL(void, sys_mic_stream_stop) {
#ifdef CONFIG_SERVICE_MIC_STREAM
  mic_stream_service_stop(pebble_task_get_current());
#endif
}
