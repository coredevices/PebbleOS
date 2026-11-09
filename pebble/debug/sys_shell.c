/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mpu.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/shell/shell.h>
#include <pbl/util/size.h>

#include <kernel/event_loop.h>
#include <kernel/util/factory_reset.h>
#include <process_management/worker_manager.h>
#include <system/bootbits.h>
#include <system/reboot_reason.h>

static int prv_cmd_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_Serial, 0};
  reboot_reason_set(&reason);
  pbl_reboot();
}

static int prv_cmd_crash(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_LauncherPanic, 0};
  reboot_reason_set(&reason);
  pbl_reboot();
}

static int prv_cmd_crash_hard(const struct pbl_shell *sh, size_t argc, char **argv) {
  RebootReason reason = {RebootReasonCode_HardFault, 0};
  reboot_reason_set(&reason);
  boot_bit_set(BOOT_BIT_FW_START_FAIL_STRIKE_TWO);
  boot_bit_set(BOOT_BIT_SOFTWARE_FAILURE_OCCURRED);
  boot_bit_clear(BOOT_BIT_FW_STABLE);
  pbl_reboot_emergency();
}

static int prv_cmd_panic(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_panic("You asked for this!");
}

#ifdef CONFIG_COREDUMP
// Known register values at a known instruction, for tests to look for in the core dump.
static void PBL_NAKED prv_fault(void) {
  __asm volatile(
      "ldr r0, =0x10101010\n"
      "ldr r1, =0x11111111\n"
      "ldr r2, =0x12121212\n"
      "ldr r3, =0x13131313\n"
      "ldr r4, =0x14141414\n"
      "ldr r5, =0x15151515\n"
      "ldr r6, =0x16161616\n"
      "ldr r7, =0x17171717\n"
      "ldr r8, =0x18181818\n"
      "ldr r9, =0x19191919\n"
      "ldr r10, =0x1a1a1a1a\n"
      "ldr r11, =0x1b1b1b1b\n"
      "ldr r12, =0x1c1c1c1c\n"
      "udf #0\n"
      ".ltorg\n");
}

static int prv_cmd_fault(const struct pbl_shell *sh, size_t argc, char **argv) {
  prv_fault();
  return 0;
}
#endif

static int prv_cmd_factory_reset(const struct pbl_shell *sh, size_t argc, char **argv) {
  if (argc < 2) {
    factory_reset(false /* should_shutdown */);
    return 0;
  }

#ifndef CONFIG_RECOVERY_FW
  if (strcmp(argv[1], "fast") == 0) {
    worker_manager_disable();

    while (worker_manager_get_current_worker_md()) {
      pbl_sleep_ms(3);
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

static uint16_t s_waste_count;
static uint16_t s_waste_delay;

static void prv_waste_time_cb(struct pbl_work *work);
static PBL_DELAYED_WORK_DEFINE(s_abusive_timer, prv_waste_time_cb);

static void prv_waste_time_cb(struct pbl_work *work) {
  for (int i = 0; i < s_waste_delay; ++i) {
    pbl_busy_wait_us(1000);
  }
  if (--s_waste_count > 0) {
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_abusive_timer, PBL_MSEC(1));
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

  s_waste_count = count;
  s_waste_delay = delay;
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_abusive_timer, PBL_MSEC(100));

  return 0;
}

#ifndef CONFIG_RELEASE
static uint64_t prv_next_tick(void) {
  uint64_t t = pbl_uptime_ticks();
  while (pbl_uptime_ticks() == t) {
  }
  return t + 1;
}

static int prv_cmd_audit_delay(const struct pbl_shell *sh, size_t argc, char **argv) {
  // Cycle counter rate against the tick, over one second.
  uint64_t t0 = prv_next_tick();
  uint32_t c0 = pbl_cycle_get_32();
  while (pbl_uptime_ticks() < t0 + PBL_TICK_HZ) {
  }
  uint32_t measured = pbl_cycle_get_32() - c0;
  uint32_t nominal = pbl_cycles_per_sec();
  int32_t ppm = (int32_t)(((int64_t)measured - nominal) * 1000000 / nominal);
  pbl_shell_print(sh, "cycles/s: nominal %" PRIu32 ", measured %" PRIu32 " (%" PRId32 " ppm)",
                  nominal, measured, ppm);

  // A long busy wait against the tick.
  t0 = prv_next_tick();
  pbl_busy_wait_us(1000000);
  uint32_t elapsed_ms = pbl_ticks_to_ms((pbl_tick_t)(pbl_uptime_ticks() - t0));
  pbl_shell_print(sh, "busy wait 1000000 us: %" PRIu32 " ms", elapsed_ms);

  // Short busy waits against the cycle counter, without interruptions.
  static uint32_t s_took_ns[500];
  pbl_irq_lock();
  pbl_busy_wait_us(1); // warm the cache
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_took_ns); i++) {
    uint32_t start = pbl_cycle_get_32();
    pbl_busy_wait_us(2 * i + 1);
    s_took_ns[i] =
        (uint32_t)(((uint64_t)(pbl_cycle_get_32() - start) * 1000000000U) / pbl_cycles_per_sec());
  }
  pbl_irq_unlock();

  uint32_t short_waits = 0;
  uint32_t max_over_ns = 0;
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_took_ns); i++) {
    uint32_t ns = (2 * i + 1) * 1000U;
    if (s_took_ns[i] < ns) {
      pbl_shell_print(sh, "busy wait %" PRIu32 " us: took %" PRIu32 " ns", ns / 1000U,
                      s_took_ns[i]);
      short_waits++;
    } else if (s_took_ns[i] - ns > max_over_ns) {
      max_over_ns = s_took_ns[i] - ns;
    }
  }
  pbl_shell_print(sh, "busy wait 1..999 us: %" PRIu32 " short, at most %" PRIu32 " ns over",
                  short_waits, max_over_ns);
  return 0;
}
#endif

static void prv_mpu_line(void *ctx, const char *line) {
  pbl_shell_print(ctx, "%s", line);
}

static int prv_cmd_mpu(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_mpu_regions_print(prv_mpu_line, (void *)sh);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_sys, mpu, nullptr, "Dump the MPU regions", prv_cmd_mpu, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, reset, nullptr, "Reset the system", prv_cmd_reset, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, crash, nullptr, "Reset as if the launcher panicked", prv_cmd_crash, 0,
                     0);
PBL_SHELL_SUBCMD_ADD(sub_sys, crash_hard, nullptr, "Hard reset as if the firmware failed to start",
                     prv_cmd_crash_hard, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_sys, panic, nullptr, "Trigger a panic", prv_cmd_panic, 0, 0);
#ifdef CONFIG_COREDUMP
PBL_SHELL_SUBCMD_ADD(sub_sys, fault, nullptr, "Trigger a fault with known registers", prv_cmd_fault,
                     0, 0);
#endif
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
#ifndef CONFIG_RELEASE
PBL_SHELL_SUBCMD_ADD(sub_sys, audit_delay, nullptr, "Audit the cycle counter and busy waits",
                     prv_cmd_audit_delay, 0, 0);
#endif

#endif
