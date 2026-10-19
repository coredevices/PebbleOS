/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall.h"

#include <time.h>

#include <pbl/drivers/rtc.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/services/time.h>
#include <pbl/util/string.h>

#include <logging/logging_private.h>
#include <process_management/worker_manager.h>

PBL_SYSCALL(int, sys_test, (VAL, int, arg)) {
  uint32_t ipsr = 0;
#ifdef __arm__
  __asm volatile("mrs %0, ipsr" : "=r"(ipsr));
#endif

  PBL_LOG_DBG("Inside test kernel function! Privileged? %s Arg %u IPSR: %" PRIu32,
              bool_to_str(!pbl_is_user_context()), arg, ipsr);

  return arg * 2;
}

PBL_SYSCALL(time_t, sys_get_time) {
  return rtc_get_time();
}

PBL_SYSCALL(void, sys_get_time_ms, (OUT, time_t *, t), (OUT, uint16_t *, out_ms)) {
  rtc_get_time_ms(t, out_ms);
}

PBL_SYSCALL(RtcTicks, sys_get_ticks) {
  return rtc_get_ticks();
}

PBL_SYSCALL(void, sys_pbl_log, (VAL, LogBinaryMessage *, log_message), (VAL, uint32_t, flags)) {
  // log_message points at a struct whose trailing message[] is sized by the
  // embedded message_length byte. Without a check, an app can hand us any
  // kernel address: log_level and message_length steer the formatting code,
  // and kernel_pbl_log_flash() ends up writing `sizeof(*log_message) +
  // message_length` bytes straight from that address to the flash log channel
  // — a generic kernel-memory exfiltration primitive.
  if (from_user) {
    kernel_pbl_log_verify_user(log_message);
  }
  kernel_pbl_log(log_message, flags);
}

PBL_SYSCALL(void, sys_copy_timezone_abbr, (OUT_BUF, char *, timezone_abbr, PBL_TZ_LEN),
            (VAL, time_t, time)) {
  time_get_timezone_abbr(timezone_abbr, time);
}

PBL_SYSCALL(struct pbl_tm *, sys_gmtime_r, (IN, const time_t *, timep),
            (OUT_BUF, struct pbl_tm *, result, sizeof(*result))) {
  return pbl_gmtime_r(timep, result);
}

PBL_SYSCALL(struct pbl_tm *, sys_localtime_r, (IN, const time_t *, timep),
            (OUT_BUF, struct pbl_tm *, result, sizeof(*result))) {
  return pbl_localtime_r(timep, result);
}

//! System call to exit an application gracefully.
PBL_SYSCALL_NORETURN(sys_exit) {
  process_manager_task_exit();
}

PBL_SYSCALL(void, sys_psleep, (VAL, int, millis)) {
  pbl_thread_sleep(PBL_MSEC(millis));
}
