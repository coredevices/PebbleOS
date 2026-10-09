/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/services/new_timer/new_timer.h>
#include <pbl/shell/shell.h>

#include <kernel/event_loop.h>
#include <kernel/util/delay.h>
#include <kernel/util/factory_reset.h>
#include <kernel/util/sleep.h>
#include <process_management/worker_manager.h>
#include <system/bootbits.h>
#include <system/passert.h>
#include <system/reboot_reason.h>
#include <system/reset.h>

// The delay audit counts Cortex-M cycles.
#if !defined(CONFIG_RELEASE) && defined(CONFIG_CPU_CORTEX_M)
#define AUDIT_DELAY 1
#include <cmsis_core.h>
#include <system/profiler.h>
#endif

static int prv_cmd_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_Serial, 0};
  reboot_reason_set(&reason);
  system_reset();
}

static int prv_cmd_crash(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_LauncherPanic, 0};
  reboot_reason_set(&reason);
  system_reset();
}

static int prv_cmd_crash_hard(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_HardFault, 0};
  reboot_reason_set(&reason);
  boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
  boot_bit_clear(BOOT_BIT_FW_STABLE);
  system_hard_reset();
}

static int prv_cmd_croak(const struct pbl_shell *sh, size_t argc, char **argv) {
  PBL_CROAK("You asked for this!");
}

static int prv_cmd_factory_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc < 2) {
    factory_reset(false /* should_shutdown */);
    return 0;
  }

#ifndef CONFIG_RECOVERY_FW
  if (strcmp(argv[1], "fast") == 0) {
    worker_manager_disable();

    while (worker_manager_get_current_worker_md()) {
      psleep(3);
    }

    launcher_task_add_callback(factory_reset_fast, nullptr);
    return 0;
  }
#endif

  pbl_shell_error(sh, "unknown mode '%s'", argv[1]);
  return -EINVAL;
}

static int prv_cmd_bootbit(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long bit;
  unsigned long value;

  if (pbl_shell_strtoul(argv[1], &bit) != 0 || bit > 31) {
    pbl_shell_error(sh, "invalid bit number '%s'", argv[1]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[2], &value) != 0 || value > 1) {
    pbl_shell_error(sh, "invalid bit value '%s', pick 1 or 0", argv[2]);
    return -EINVAL;
  }

  if (value == 0) {
    boot_bit_clear(1U << bit);
  } else {
    boot_bit_set(1U << bit);
  }

  return 0;
}

static TimerID s_abusive_timer = TIMER_INVALID_ID;

struct WasteTimerData {
  uint16_t count;
  uint16_t delay;
};
static_assert(sizeof(struct WasteTimerData) <= sizeof(uintptr_t), "struct WasteTimerData too big");

static void prv_waste_time_cb(void *context) {
  struct WasteTimerData data;
  memcpy(&data, &context, sizeof data);

  for (int i = 0; i < data.delay; ++i) {
    delay_us(1000);
  }
  if (--data.count > 0) {
    memcpy(&context, &data, sizeof context);
    new_timer_start(s_abusive_timer, 1, prv_waste_time_cb, context, 0);
  }
}

static int prv_cmd_waste_time(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long count;
  unsigned long delay;

  if (pbl_shell_strtoul(argv[1], &count) != 0 || count == 0 || count > 0xFFFF) {
    pbl_shell_error(sh, "invalid count '%s'", argv[1]);
    return -EINVAL;
  }

  if (pbl_shell_strtoul(argv[2], &delay) != 0 || delay == 0 || delay > 0xFFFF) {
    pbl_shell_error(sh, "invalid delay '%s'", argv[2]);
    return -EINVAL;
  }

  struct WasteTimerData data = {count, delay};
  uintptr_t data_pack = 0;
  memcpy(&data_pack, &data, sizeof data);

  if (s_abusive_timer == TIMER_INVALID_ID) {
    s_abusive_timer = new_timer_create();
  }
  if (!new_timer_start(s_abusive_timer, 100, prv_waste_time_cb, (void *)data_pack, 0)) {
    pbl_shell_error(sh, "failed to start the timer");
    return -EIO;
  }

  return 0;
}

#ifdef AUDIT_DELAY
static int prv_cmd_audit_delay(const struct pbl_shell *sh, size_t argc, char **argv) {
  profiler_init();

  // don't let context switches skew our results
  __disable_irq();

  // test short delays because we should really be using psleep() for longer stalls!
  for (uint32_t i = 1; i <= 1000; i += 2) {
    profiler_start();
    delay_us(i);
    profiler_stop();
    uint32_t duration_us = profiler_get_total_duration(true);

    // at least the requested time, and no more than 5% over it
    bool passed = ((duration_us >= i) && (duration_us <= ((i * 105) / 100)));
    if (!passed) {
      pbl_shell_print(sh, "audit failed: expected %" PRIu32 ", got %" PRIu32, i, duration_us);
    }
  }

  __enable_irq();
  pbl_shell_print(sh, "delay_us audit complete");
  return 0;
}
#endif

PBL_SHELL_SUBCMD_ADD(sub_sys, reset, nullptr, "Reset the system", prv_cmd_reset, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, crash, nullptr, "Reset as if the launcher panicked", prv_cmd_crash, 0,
                     0);
PBL_SHELL_SUBCMD_ADD(sub_sys, crash_hard, nullptr, "Hard reset as if the firmware failed to start",
                     prv_cmd_crash_hard, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, croak, nullptr, "Trigger a croak", prv_cmd_croak, 0, 0);
#ifdef CONFIG_RECOVERY_FW
PBL_SHELL_SUBCMD_ADD(sub_sys, factory_reset, nullptr, "Factory reset", prv_cmd_factory_reset, 1, 0);
#else
PBL_SHELL_SUBCMD_ADD(sub_sys, factory_reset, nullptr, "Factory reset [fast]", prv_cmd_factory_reset,
                     1, 1);
#endif
PBL_SHELL_SUBCMD_ADD(sub_sys, bootbit, nullptr, "Set a boot bit <bit> <0|1>", prv_cmd_bootbit, 3,
                     0);
PBL_SHELL_SUBCMD_ADD(sub_sys, waste_time, nullptr, "Busy-wait on the timer task <count> <delay_ms>",
                     prv_cmd_waste_time, 3, 0);
#ifdef AUDIT_DELAY
PBL_SHELL_SUBCMD_ADD(sub_sys, audit_delay, nullptr, "Audit the accuracy of delay_us",
                     prv_cmd_audit_delay, 0, 0);
#endif

#endif
