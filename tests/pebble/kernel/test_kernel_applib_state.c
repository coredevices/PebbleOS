/* SPDX-FileCopyrightText: 2026 Yusuf Sevim */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/sched.h>

#include <clar.h>
#include <fake_mutex.h>
#include <kernel/kernel_applib_state.h>
#include <kernel/pebble_tasks.h>

static struct pbl_thread *s_kernel_main_thread;
static PebbleTask s_current_task;
static bool s_kernel_running;
static bool s_irq_locked;

struct pbl_thread *pebble_task_get_thread(PebbleTask task) {
  return (task == PebbleTask_KernelMain) ? s_kernel_main_thread : nullptr;
}

PebbleTask pebble_task_get_current(void) {
  return s_current_task;
}

bool pbl_kernel_is_running(void) {
  return s_kernel_running;
}

bool pbl_irq_is_locked(void) {
  return s_irq_locked;
}

void connection_service_state_init(ConnectionServiceState *state) {
}

void battery_state_service_state_init(BatteryStateServiceState *state) {
}

void test_kernel_applib_state__initialize(void) {
  fake_mutex_reset(false);
  s_kernel_main_thread = nullptr;
  s_current_task = PebbleTask_KernelMain;
  s_kernel_running = false;
  s_irq_locked = false;
}

void test_kernel_applib_state__cleanup(void) {
  cl_assert(fake_mutex_all_unlocked());
}

static void prv_boot(void) {
  s_kernel_running = true;
  s_kernel_main_thread = (struct pbl_thread *)&s_kernel_main_thread;
  kernel_applib_init();
}

void test_kernel_applib_state__log_before_the_kernel_takes_no_mutex(void) {
  LogState *state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  cl_assert(fake_mutex_all_unlocked());
  kernel_applib_release_log_state(state);
}

void test_kernel_applib_state__log_of_a_thread_holds_the_mutex(void) {
  prv_boot();
  s_current_task = PebbleTask_SysWorkq;

  LogState *state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  cl_assert(!fake_mutex_all_unlocked());
  cl_assert(kernel_applib_get_log_state() == nullptr);

  kernel_applib_release_log_state(state);
  cl_assert(fake_mutex_all_unlocked());
}

void test_kernel_applib_state__log_with_interrupts_locked_takes_no_mutex(void) {
  prv_boot();
  s_irq_locked = true;

  LogState *state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  cl_assert(fake_mutex_all_unlocked());

  s_irq_locked = false;
  kernel_applib_release_log_state(state);
}

void test_kernel_applib_state__log_across_start_releases_what_it_took(void) {
  s_kernel_running = true;
  s_current_task = PebbleTask_SysWorkq;

  LogState *state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  cl_assert(fake_mutex_all_unlocked());

  s_kernel_main_thread = (struct pbl_thread *)&s_kernel_main_thread;
  kernel_applib_init();

  kernel_applib_release_log_state(state);

  state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  kernel_applib_release_log_state(state);

  s_current_task = PebbleTask_KernelMain;
  state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  kernel_applib_release_log_state(state);
}

void test_kernel_applib_state__log_of_a_thread_without_a_task(void) {
  prv_boot();
  s_current_task = PebbleTask_Unknown;

  LogState *state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  cl_assert(!fake_mutex_all_unlocked());
  cl_assert(kernel_applib_get_log_state() == nullptr);

  kernel_applib_release_log_state(state);
  cl_assert(fake_mutex_all_unlocked());

  state = kernel_applib_get_log_state();
  cl_assert(state != nullptr);
  kernel_applib_release_log_state(state);
}
