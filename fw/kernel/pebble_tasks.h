/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/msgq.h>
#include <pbl/kernel/thread.h>

#include <kernel/memory_layout.h>

//! This is an enumeration of different tasks we've had in our system. Please don't rearrange
//! these numbers! For example, the value of PebbleTask_Timers is hardcoded into our syscall
//! assembly and terrible things will happen if you move this around.
typedef enum PebbleTask {
  PebbleTask_KernelMain,
  PebbleTask_KernelBackground,
  PebbleTask_Worker,
  PebbleTask_App,

  PebbleTask_BTHost,       // Bluetooth Host
  PebbleTask_BTController, // Bluetooth Controller
  PebbleTask_BTHCI,        // Bluetooth HCI

  PebbleTask_NewTimers,

  PebbleTask_PULSE,

  NumPebbleTask,

  PebbleTask_Unknown
} PebbleTask;

typedef uint16_t PebbleTaskBitset;

static_assert((1 << (8 * sizeof(PebbleTaskBitset))) >= (1 << NumPebbleTask),
              "The type of PebbleTaskBitset is not wide enough to "
              "track all tasks in the PebbleTask enum");

void pebble_task_register(PebbleTask task, struct pbl_thread *thread);
void pebble_task_unregister(PebbleTask task);

const char *pebble_task_get_name(PebbleTask task);

//! @return a single character that indicates the task
char pebble_task_get_char(PebbleTask task);

PebbleTask pebble_task_get_current(void);

PebbleTask pebble_task_get_task_for_thread(const struct pbl_thread *thread);

//! @return the thread running @p task, or NULL while it is not running.
struct pbl_thread *pebble_task_get_thread(PebbleTask task);

void pebble_task_suspend(PebbleTask task);

//! @return The queue handle to send events to the given task.
struct pbl_msgq *pebble_task_get_to_queue(PebbleTask task);

//! Creates the thread for @p pebble_task with the memory regions it needs;
//! @p attr->regions is filled in here.
struct pbl_thread *pebble_task_create(PebbleTask pebble_task, struct pbl_thread_attr *attr);

#ifdef CONFIG_USERSPACE
//! Runs the syscalls of @p task (App or Worker) on its dedicated privileged
//! stack instead of the caller's.
void pebble_task_attr_set_syscall_stack(PebbleTask task, struct pbl_thread_attr *attr);
#endif

void pebble_task_configure_idle_task(void);

//! Allow or forbid instruction fetches from App RAM on the running App task.
//! Forbidding them makes privileged code fault if it jumps into app code.
void pebble_task_set_app_ram_executable(bool executable);

#if UNITTEST
#define PBL_ASSERT_TASK(task)
#define PBL_ASSERT_NOT_TASK(task)
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task)
#else
void pebble_task_assert_current(PebbleTask expected_task);
void pebble_task_assert_not_current(PebbleTask unexpected_task);

//! Panic unless running from @p task.
#define PBL_ASSERT_TASK(task)     pebble_task_assert_current(task)
//! Panic when running from @p task.
#define PBL_ASSERT_NOT_TASK(task) pebble_task_assert_not_current(task)

// Only checked when CHECK_RUNNING_FROM_EXPECTED_TASK is defined, to save code space.
#ifdef CHECK_RUNNING_FROM_EXPECTED_TASK
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task) PBL_ASSERT_TASK(task)
#else
#define PBL_ASSERT_RUNNING_FROM_EXPECTED_TASK(task)
#endif
#endif
