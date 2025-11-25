/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/memory_layout.h"
#include "process_management/app_manager.h"
#include "process_management/worker_manager.h"
#include "process_state/app_state/app_state.h"
#include "services/common/compositor/compositor.h"
#include "services/common/event_service.h"
#include "syscall/syscall_internal.h"
#include "system/logging.h"
#include "system/passert.h"

static void prv_put_event_from_process(PebbleTask task, PebbleEvent *event) {
  if (!event_try_put_from_process(task, event)) {
    PBL_LOG(LOG_LEVEL_WARNING, "%s: From app queue is full! Dropped %p! Killing App",
        (task == PebbleTask_App ? "App" : "Worker"), event);
    syscall_failed();
  }
}

DEFINE_SYSCALL(void, sys_send_pebble_event_to_kernel, PebbleEvent* event) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(event, sizeof(*event));
  }

  PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_App || task == PebbleTask_Worker) {
    prv_put_event_from_process(task, event);
  } else {
    event_put(event);
  }
}

DEFINE_SYSCALL(void, sys_current_process_schedule_callback,
               CallbackEventCallback async_cb, void *ctx) {
  // No userspace buffer assertion for ctx needed, because it won't be accessed by the kernel.

  PebbleEvent event = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = async_cb,
      .data = ctx,
    },
  };
  const PebbleTask task = pebble_task_get_current();
  PBL_ASSERTN(task == PebbleTask_App || task == PebbleTask_Worker);
  process_manager_send_event_to_process(task, &event);
}

DEFINE_SYSCALL(uint32_t, sys_process_events_waiting, PebbleTask task) {
  return process_manager_process_events_waiting(task);
}

DEFINE_SYSCALL(void, sys_event_service_client_subscribe, EventServiceInfo *handler) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(handler, sizeof(*handler));
  }

  PebbleTask task = pebble_task_get_current();

  // Get info
  QueueHandle_t *event_queue;
  if (task == PebbleTask_App) {
    event_queue = app_manager_get_task_context()->to_process_event_queue;
  } else if (task == PebbleTask_Worker) {
    event_queue = worker_manager_get_task_context()->to_process_event_queue;
  } else if (task == PebbleTask_KernelMain) {
    // The event service always runs from KernelMain
    event_queue = event_kernel_to_kernel_event_queue();
  } else {
    WTF;
  }

  // Subscribe to the service!
  PebbleEvent event = {
    .type = PEBBLE_SUBSCRIPTION_EVENT,
    .subscription = {
      .subscribe = true,
      .task = task,
      .event_queue = event_queue,
      .event_type = handler->type,
    },
  };
  if (task == PebbleTask_KernelMain) {
    // The client is also KernelMain, just subscribe immediately without putting an event
    event_service_subscribe_from_kernel_main(&event.subscription);
  } else {
    prv_put_event_from_process(task, &event);
  }
}

DEFINE_SYSCALL(void, sys_event_service_client_unsubscribe, EventServiceInfo *state,
                                                           EventServiceInfo *handler) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(handler, sizeof(*handler));
    syscall_assert_userspace_buffer(state, sizeof(*state));
  }

  // Remove from handlers list
  list_remove(&handler->list_node, NULL, NULL);

  if (list_find(&state->list_node, event_service_filter, (void *) handler->type)) {
    // there are other handlers for this task, don't unsubscribe it
    return;
  }
  // Get info
  PebbleTask task = pebble_task_get_current();
  // Unsubscribe from the service!
  PebbleEvent event = {
    .type = PEBBLE_SUBSCRIPTION_EVENT,
    .subscription = {
      .subscribe = false,
      .task = task,
      .event_type = handler->type,
    },
  };
  prv_put_event_from_process(task, &event);
}
