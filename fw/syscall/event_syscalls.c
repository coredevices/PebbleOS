/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/logging/logging.h>
#include <pbl/services/event_service.h>

#include <kernel/kernel_applib_state.h>
#include <kernel/memory_layout.h>
#include <process_management/app_manager.h>
#include <process_management/worker_manager.h>
#include <process_state/app_state/app_state.h>
#include <process_state/worker_state/worker_state.h>
#include <syscall/syscall_define.h>
#include <system/passert.h>

static void prv_put_event_from_process(PebbleTask task, PebbleEvent *event) {
  if (!event_try_put_from_process(task, event)) {
    PBL_LOG_WRN("%s: From app queue is full! Dropped %p! Killing App",
                (task == PebbleTask_App ? "App" : "Worker"), event);
    syscall_failed();
  }
}

// Event types that an unprivileged app or worker is permitted to post directly via
// sys_send_pebble_event_to_kernel. Anything else (in particular events that carry a
// kernel-invoked callback pointer, like PEBBLE_CALLBACK_EVENT) is forbidden — the
// kernel would otherwise jump to an app-controlled address with kernel privileges.
static bool prv_event_type_allowed_from_user(PebbleEventType type) {
  switch (type) {
    case PEBBLE_PROCESS_KILL_EVENT:
    case PEBBLE_RENDER_READY_EVENT:
    case PEBBLE_DICTATION_EVENT:
    case PEBBLE_HEALTH_SERVICE_EVENT:
    case PEBBLE_PLUGIN_SERVICE_EVENT:
      return true;
    default:
      return false;
  }
}

static const MpuRegion *prv_process_region(PebbleTask task) {
  return (task == PebbleTask_Worker) ? memory_layout_get_worker_region()
                                     : memory_layout_get_app_region();
}

PBL_SYSCALL(void, sys_send_pebble_event_to_kernel, (IN, PebbleEvent *, event)) {
  PebbleTask task = pebble_task_get_current();

  if (from_user) {
    if (!prv_event_type_allowed_from_user(event->type)) {
      PBL_LOG_ERR("Rejecting event type %u from unprivileged caller", event->type);
      syscall_failed();
    }

    if (event->type == PEBBLE_PROCESS_KILL_EVENT) {
      event->kill.task = task;
    } else if (event->type == PEBBLE_DICTATION_EVENT && event->dictation.text != nullptr &&
               !memory_layout_is_cstring_in_region(prv_process_region(task), event->dictation.text,
                                                   SIZE_MAX)) {
      syscall_failed();
    }
  }

  if (task == PebbleTask_App || task == PebbleTask_Worker) {
    prv_put_event_from_process(task, event);
  } else {
    event_put(event);
  }
}

PBL_SYSCALL(void, sys_current_process_schedule_callback, (VAL, CallbackEventCallback, async_cb),
            (VAL, void *, ctx)) {
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

PBL_SYSCALL(uint32_t, sys_process_events_waiting, (VAL, PebbleTask, task)) {
  // process_manager_process_events_waiting -> prv_get_context_for_task
  // PBL_ASSERTN()s for anything other than App or Worker. Without this guard
  // an unprivileged app can pass any other PebbleTask value and panic the
  // kernel from a syscall. Pin the caller to its own task instead.
  if (from_user) {
    task = pebble_task_get_current();
  }
  return process_manager_process_events_waiting(task);
}

// The handler structure is supplied by the app and its embedded ListNode pointers
// flow into kernel-mode list operations (notably list_remove on unsubscribe). Without
// validating the prev/next pointers, an app can craft them to point into kernel
// memory and use list_remove as a 4-byte arbitrary-write primitive (`*a = b; *b = a`).
static void prv_assert_list_node_in_userspace(const ListNode *node) {
  if (node != nullptr) {
    syscall_assert_userspace_buffer(node, sizeof(*node));
  }
}

static void prv_assert_handler_list_node_in_userspace(const EventServiceInfo *handler) {
  prv_assert_list_node_in_userspace(handler->list_node.prev);
  prv_assert_list_node_in_userspace(handler->list_node.next);
}

PBL_SYSCALL(void, sys_event_service_client_subscribe,
            (IN_BUF, EventServiceInfo *, handler, sizeof(*handler))) {
  if (from_user) {
    prv_assert_handler_list_node_in_userspace(handler);
    if (handler->type >= PEBBLE_NUM_EVENTS) {
      syscall_failed();
    }
  }

  PebbleTask task = pebble_task_get_current();

  // Get info
  struct pbl_msgq *event_queue;
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

static bool prv_has_handler_for_type(EventServiceInfo *state, PebbleEventType type,
                                     bool from_userspace) {
  ListNode *slow = &state->list_node;
  bool advance_slow = false;
  for (ListNode *node = &state->list_node; node != nullptr; node = node->next) {
    if (from_userspace) {
      syscall_assert_userspace_buffer(node, sizeof(EventServiceInfo));
    }
    if (event_service_filter(node, (void *)(uintptr_t)type)) {
      return true;
    }
    if (advance_slow) {
      slow = slow->next;
    }
    advance_slow = !advance_slow;
    if (node->next == slow) {
      // Cyclic list
      if (from_userspace) {
        syscall_failed();
      }
      return false;
    }
  }
  return false;
}

PBL_SYSCALL(void, sys_event_service_client_unsubscribe, (VAL, EventServiceInfo *, state),
            (OUT_BUF, EventServiceInfo *, handler, sizeof(*handler))) {
  PebbleTask task = pebble_task_get_current();

  if (from_user) {
    prv_assert_handler_list_node_in_userspace(handler);
    if (handler->type >= PEBBLE_NUM_EVENTS) {
      syscall_failed();
    }
    // The user-supplied `state` pointer flows into list_find as the list head; an app
    // could craft it (and the chain it heads) to walk into kernel memory. Re-derive
    // the head from authoritative per-process state instead of trusting the caller.
    if (task == PebbleTask_App) {
      state = app_state_get_event_service_state();
    } else if (task == PebbleTask_Worker) {
      state = worker_state_get_event_service_state();
    } else {
      WTF;
    }
  }

  // Remove from handlers list
  list_remove(&handler->list_node, nullptr, nullptr);

  if (prv_has_handler_for_type(state, handler->type, from_user)) {
    // there are other handlers for this task, don't unsubscribe it
    return;
  }
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
