/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/assert.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>
#ifdef CONFIG_TASK_WDT
#include <pbl/task_wdt/task_wdt.h>
#endif

#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>

PBL_LOG_MODULE_DEFINE(nimble, CONFIG_NIMBLE_LOG_LEVEL);

static struct pbl_thread s_host_thread;
PBL_THREAD_STACK_DEFINE(s_host_stack, CONFIG_NIMBLE_HOST_THREAD_STACK_SIZE);

#if NIMBLE_CFG_CONTROLLER
static struct pbl_thread s_ll_thread;
PBL_THREAD_STACK_DEFINE(s_ll_stack, CONFIG_NIMBLE_LL_THREAD_STACK_SIZE);
#endif

static void prv_host_main(void *arg) {
  struct ble_npl_eventq *evq = nimble_port_get_dflt_eventq();

#ifdef CONFIG_TASK_WDT
  PBL_ASSERTN(pbl_task_wdt_add(nullptr, CONFIG_TASK_WDT_TIMEOUT_MS, nullptr, nullptr) >= 0);
#endif

  while (true) {
#ifdef CONFIG_TASK_WDT
    pbl_task_wdt_set_waiting(true);
#endif
    struct ble_npl_event *ev = ble_npl_eventq_get(evq, BLE_NPL_TIME_FOREVER);
#ifdef CONFIG_TASK_WDT
    pbl_task_wdt_set_waiting(false);
#endif
    ble_npl_event_run(ev);
  }
}

void nimble_port_pebble_init(void) {
  struct pbl_thread_attr host_attr = {
    .name = "NimbleHost",
    .entry = prv_host_main,
    .prio = CONFIG_NIMBLE_HOST_THREAD_PRIO,
    .privileged = true,
    .stack = s_host_stack,
    .stack_size = sizeof(s_host_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_host_thread, &host_attr) == 0);

#if NIMBLE_CFG_CONTROLLER
  struct pbl_thread_attr ll_attr = {
    .name = "NimbleLL",
    .entry = nimble_port_ll_task_func,
    .prio = CONFIG_NIMBLE_LL_THREAD_PRIO,
    .privileged = true,
    .stack = s_ll_stack,
    .stack_size = sizeof(s_ll_stack),
  };

  PBL_ASSERTN(pbl_thread_create(&s_ll_thread, &ll_attr) == 0);
#endif
}

struct pbl_thread *nimble_port_pebble_host_thread(void) {
  return &s_host_thread;
}

#if NIMBLE_CFG_CONTROLLER
struct pbl_thread *nimble_port_pebble_ll_thread(void) {
  return &s_ll_thread;
}
#endif
