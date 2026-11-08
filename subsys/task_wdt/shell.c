/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <errno.h>
#include <string.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/shell/shell.h>

#include <kernel/event_loop.h>
#include <kernel/pebble_tasks.h>

static void prv_stall(void *data) {
  PBL_LOG_WRN("Stalling %s", pebble_task_get_name(pebble_task_get_current()));
  for (;;) {
  }
}

static void prv_stall_irq(void *data) {
  // Nothing runs, not even the watchdog thread: the hardware watchdog has to reset us.
  pbl_irq_lock();
  prv_stall(data);
}

static void prv_stall_work(struct pbl_work *work) {
  prv_stall(nullptr);
}

static PBL_DELAYED_WORK_DEFINE(s_stall_timer, prv_stall_work);

static int prv_cmd_stall(const struct pbl_shell *sh, size_t argc, char **argv) {
  const char *thread = argv[1];

  if (strcmp(thread, "main") == 0) {
    launcher_task_add_callback(prv_stall, nullptr);
  } else if (strcmp(thread, "timers") == 0) {
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_stall_timer, PBL_MSEC(10));
  } else if (strcmp(thread, "bg") == 0) {
    pbl_sys_work_call(prv_stall, nullptr);
  } else if (strcmp(thread, "irq") == 0) {
    pbl_sys_work_call(prv_stall_irq, nullptr);
  } else {
    pbl_shell_error(sh, "unknown thread '%s', pick main | bg | timers | irq", thread);
    return -EINVAL;
  }

  return 0;
}

static const struct pbl_shell_cmd sub_wdt[] = {
  PBL_SHELL_CMD_ARG(stall, nullptr, "Spin a thread forever <main|bg|timers|irq>", prv_cmd_stall, 2,
                    0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(wdt, sub_wdt, "Task watchdog", nullptr);

#endif
