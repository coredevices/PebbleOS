/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/applib_malloc.auto.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#include "process_management/process_manager.h"
#include "services/common/new_timer/new_timer.h"
#include "services/common/system_task.h"
#include "services/normal/accessory/smartstrap_attribute.h"
#include "services/normal/accessory/smartstrap_comms.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_profiles.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "os/mutex.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/list.h"
#include "util/mbuf.h"

//! Currently, we only support attributes being created by the App task
#define CONSUMER_TASK PebbleTask_App
//! Gets the next attribute in the list
#define NEXT_ATTR(attr) ((SmartstrapAttributeInternal *)list_get_next(&attr->list_node))
//! This macro allows easy iteration through attributes in the s_attr_head list which don't have the
//! 'deferred_delete' field set. Example usage:
//!   SmartstrapAttributeInternal *attr;
//!   FOREACH_VALID_ATTR(attr) {
//!     // 'attr' will be the current item in the list
//!   }
#define FOREACH_VALID_ATTR(item) \
  for (item = s_attr_head; item; item = NEXT_ATTR(item)) \
    if (!item->deferred_delete)

// This file relies on the ServiceId/AttributeId being uint16_t as the protocol defines it as such.
_Static_assert(sizeof(SmartstrapServiceId) == sizeof(uint16_t),
               "SmartstrapServiceId MUST be two bytes in length!");
_Static_assert(sizeof(SmartstrapAttributeId) == sizeof(uint16_t),
               "SmartstrapAttributeId MUST be two bytes in length!");

typedef struct {
  ListNode list_node;
  //! The ServiceId for this attribute
  uint16_t service_id;
  //! The AttributeId for this attribute
  uint16_t attribute_id;
  //! MBuf used for sending / receiving data for this attribute
  MBuf mbuf;
  //! The number of bytes to write from the buffer
  uint32_t write_length;
  //! The type of request which is currently pending
  SmartstrapRequestType request_type:8;
  //! The current state of this attribute
  SmartstrapAttributeState state:8;
  //! The timeout to use for the next request
  uint16_t timeout_ms;
  //! Whether or not writes are being blocked
  bool write_blocked;
  //! Whether or not this attribute has a deferred delete pending
  bool deferred_delete;
} SmartstrapAttributeInternal;

typedef struct {
  uint16_t service_id;
  uint16_t attribute_id;
} AttributeFilterContext;

typedef enum {
  AttributeTransactionRead,
  AttributeTransactionBeginWrite,
  AttributeTransactionEndWrite
} AttributeTransaction;

static SmartstrapAttributeInternal *s_attr_head;
static bool s_deferred_delete_queued;
static PebbleMutex *s_attr_list_lock;


// Init
////////////////////////////////////////////////////////////////////////////////

void smartstrap_attribute_init(void) {
  s_attr_list_lock = mutex_create();
}


// Attribute state functions
////////////////////////////////////////////////////////////////////////////////

static bool prv_is_valid_fsm_transition(SmartstrapAttributeInternal *attr,
                                        SmartstrapAttributeState new_state) {
  SmartstrapAttributeState current_state = attr->state;
  if ((current_state == SmartstrapAttributeStateIdle) &&
      (new_state == SmartstrapAttributeStateRequestPending)) {
    return pebble_task_get_current() == CONSUMER_TASK;
  } else if ((current_state == SmartstrapAttributeStateIdle) &&
             (new_state == SmartstrapAttributeStateWritePending)) {
    return pebble_task_get_current() == CONSUMER_TASK;
  } else if ((current_state == SmartstrapAttributeStateWritePending) &&
             (new_state == SmartstrapAttributeStateRequestPending)) {
    return pebble_task_get_current() == CONSUMER_TASK;
  } else if ((current_state == SmartstrapAttributeStateWritePending) &&
             (new_state == SmartstrapAttributeStateIdle)) {
    return pebble_task_get_current() == CONSUMER_TASK;
  } else if ((current_state == SmartstrapAttributeStateRequestPending) &&
             (new_state == SmartstrapAttributeStateIdle)) {
    return pebble_task_get_current() == PebbleTask_KernelBackground;
  } else if ((current_state == SmartstrapAttributeStateRequestPending) &&
             (new_state == SmartstrapAttributeStateRequestInProgress)) {
    return pebble_task_get_current() == PebbleTask_KernelBackground;
  } else if ((current_state == SmartstrapAttributeStateRequestInProgress) &&
             (new_state == SmartstrapAttributeStateIdle)) {
    return pebble_task_get_current() == PebbleTask_KernelBackground;
  } else {
    return false;
  }
}

static void prv_set_attribute_state(SmartstrapAttributeInternal *attr,
                                    SmartstrapAttributeState new_state) {
  PBL_ASSERTN(prv_is_valid_fsm_transition(attr, new_state));
  attr->state = new_state;
}

//! Note; This should only be called from the consumer task
static bool prv_start_transaction(SmartstrapAttributeInternal *attr, AttributeTransaction txn) {
  PBL_ASSERT_TASK(CONSUMER_TASK);
  if ((txn == AttributeTransactionRead) && (attr->state == SmartstrapAttributeStateIdle)) {
    prv_set_attribute_state(attr, SmartstrapAttributeStateRequestPending);
    return true;
  } else if ((txn == AttributeTransactionBeginWrite) &&
             (attr->state == SmartstrapAttributeStateIdle)) {
    prv_set_attribute_state(attr, SmartstrapAttributeStateWritePending);
    return true;
  } else if ((txn == AttributeTransactionEndWrite) &&
             (attr->state == SmartstrapAttributeStateWritePending)) {
    prv_set_attribute_state(attr, SmartstrapAttributeStateRequestPending);
    return true;
  }
  return false;
}

//! Note; This should only be called from the consumer task
static void prv_cancel_transaction(SmartstrapAttributeInternal *attr) {
  PBL_ASSERT_TASK(CONSUMER_TASK);
  if (attr->state == SmartstrapAttributeStateWritePending) {
    prv_set_attribute_state(attr, SmartstrapAttributeStateIdle);
  }
}


// List searching functions
////////////////////////////////////////////////////////////////////////////////

//! NOTE: the caller must hold s_attr_list_lock
static SmartstrapAttributeInternal *prv_find_by_ids(uint16_t service_id, uint16_t attribute_id) {
  mutex_assert_held_by_curr_task(s_attr_list_lock, true);
  SmartstrapAttributeInternal *attr;
  FOREACH_VALID_ATTR(attr) {
    if ((attr->service_id == service_id) && (attr->attribute_id == attribute_id)) {
      return attr;
    }
  }
  return NULL;
}

//! NOTE: the caller must hold s_attr_list_lock
static SmartstrapAttributeInternal *prv_find_by_buffer(uint8_t *buffer) {
  mutex_assert_held_by_curr_task(s_attr_list_lock, true);
  SmartstrapAttributeInternal *attr;
  FOREACH_VALID_ATTR(attr) {
    if (mbuf_get_data(&attr->mbuf) == buffer) {
      return attr;
    }
  }
  return NULL;
}

//! NOTE: the caller must hold s_attr_list_lock
static SmartstrapAttributeInternal *prv_find_by_state(SmartstrapAttributeState state) {
  mutex_assert_held_by_curr_task(s_attr_list_lock, true);
  SmartstrapAttributeInternal *attr;
  FOREACH_VALID_ATTR(attr) {
    if (attr->state == state) {
      return attr;
    }
  }
  return NULL;
}


// Attribute processing / request functions
// NOTE: These all run on KernelBG which moves attributes from the RequestPending to the Idle state
////////////////////////////////////////////////////////////////////////////////

bool smartstrap_attribute_send_pending(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  mutex_lock(s_attr_list_lock);
  if (prv_find_by_state(SmartstrapAttributeStateRequestInProgress)) {
    // we already have a request in progress
    mutex_unlock(s_attr_list_lock);
    return false;
  }

  // get the next attribute which has a pending request
  SmartstrapAttributeInternal *attr = prv_find_by_state(SmartstrapAttributeStateRequestPending);
  mutex_unlock(s_attr_list_lock);
  if (!attr) {
    return false;
  }

  // prepare the request
  PBL_ASSERTN(!mbuf_get_next(&attr->mbuf));
  MBuf write_mbuf = MBUF_EMPTY;
  mbuf_set_data(&write_mbuf, mbuf_get_data(&attr->mbuf), attr->write_length);
  SmartstrapRequest request = (SmartstrapRequest) {
    .service_id = attr->service_id,
    .attribute_id = attr->attribute_id,
    .write_mbuf = (attr->request_type == SmartstrapRequestTypeRead) ? NULL : &write_mbuf,
    .read_mbuf = (attr->request_type == SmartstrapRequestTypeWrite) ? NULL : &attr->mbuf,
    .timeout_ms = attr->timeout_ms
  };

  // send the request
  SmartstrapResult result = smartstrap_profiles_handle_request(&request);
  if (result == SmartstrapResultBusy) {
    // there was another request in progress so we'll try again later
    return false;
  } else if ((result == SmartstrapResultOk) &&
             (smartstrap_fsm_state_get() != SmartstrapStateReadReady)) {
    prv_set_attribute_state(attr, SmartstrapAttributeStateRequestInProgress);
    if (attr->request_type == SmartstrapRequestTypeWrite) {
      // This is a generic service write, which will be ACK'd by the smartstrap so we shouldn't
      // send the event yet.
      return true;
    }
  } else {
    // Either the request was not written successfully, or we are not waiting for a response for it.
    prv_set_attribute_state(attr, SmartstrapAttributeStateIdle);
  }

  // send an event now that we've completed the write
  PebbleEvent event = {
    .type = PEBBLE_SMARTSTRAP_EVENT,
    .smartstrap = {
      .type = SmartstrapDataSentEvent,
      .result = result,
      .attribute = mbuf_get_data(&attr->mbuf)
    },
  };
  process_manager_send_event_to_process(CONSUMER_TASK, &event);
  return true;
}

void smartstrap_attribute_send_event(SmartstrapEventType type, SmartstrapProfile profile,
                                     SmartstrapResult result, uint16_t service_id,
                                     uint16_t attribute_id, uint16_t read_length) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  PebbleEvent event = {
    .type = PEBBLE_SMARTSTRAP_EVENT,
    .smartstrap = {
      .type = type,
      .profile = profile,
      .result = result,
      .read_length = read_length,
    },
  };
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = prv_find_by_ids(service_id, attribute_id);
  mutex_unlock(s_attr_list_lock);
  if (!attr) {
    // this attribute has likely since been destroyed
    return;
  }
  event.smartstrap.attribute = mbuf_get_data(&attr->mbuf);
  if (type == SmartstrapDataReceivedEvent) {
    PBL_ASSERTN(attr->state == SmartstrapAttributeStateRequestInProgress);
    prv_set_attribute_state(attr, SmartstrapAttributeStateIdle);
    if (attr->request_type == SmartstrapRequestTypeWrite) {
      // the data we got was the ACK of the write, so change the event type and don't block writes
      event.smartstrap.type = SmartstrapDataSentEvent;
    } else {
      // prevent writing to the attribute until the app handles the event, at which point applib
      // code will call sys_smartstrap_attribute_event_processed() to clear this flag
      attr->write_blocked = true;
    }
    process_manager_send_event_to_process(CONSUMER_TASK, &event);
  } else if (type == SmartstrapNotifyEvent) {
    process_manager_send_event_to_process(CONSUMER_TASK, &event);
  }
}

static void prv_do_deferred_delete_cb(void *context) {
  s_deferred_delete_queued = false;
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = s_attr_head;
  while (attr) {
    SmartstrapAttributeInternal *next = NEXT_ATTR(attr);
    if (attr->deferred_delete) {
      list_remove(&attr->list_node, (ListNode **)&s_attr_head, NULL);
      kernel_free(attr);
    }
    attr = next;
  }
  mutex_unlock(s_attr_list_lock);
}


// Syscalls
// NOTE: These all run on the consumer task which moves attributes from Idle to RequestPending
////////////////////////////////////////////////////////////////////////////////

DEFINE_SYSCALL(bool, sys_smartstrap_attribute_register, uint16_t service_id,
               uint16_t attribute_id, uint8_t *buffer, size_t buffer_length) {
  if (PRIVILEGE_WAS_ELEVATED) {
    syscall_assert_userspace_buffer(buffer, buffer_length);
  }
  if (buffer_length > SMARTSTRAP_ATTRIBUTE_LENGTH_MAXIMUM) {
    PBL_LOG(LOG_LEVEL_ERROR, "Attribute length of %"PRIu32" is too long", (uint32_t)buffer_length);
    return false;
  }

  mutex_lock(s_attr_list_lock);
  const bool exists = (prv_find_by_ids(service_id, attribute_id) || prv_find_by_buffer(buffer));
  mutex_unlock(s_attr_list_lock);
  if (exists) {
    PBL_LOG(LOG_LEVEL_ERROR, "Attribute already exists (0x%x,0x%x)", service_id, attribute_id);
    return false;
  }

  SmartstrapAttributeInternal *new_attr = kernel_zalloc(sizeof(SmartstrapAttributeInternal));
  if (!new_attr) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to allocate attribute");
    return false;
  }

  *new_attr = (SmartstrapAttributeInternal) {
    .service_id = service_id,
    .attribute_id = attribute_id,
    .mbuf = MBUF_EMPTY
  };
  list_init(&new_attr->list_node);
  mbuf_set_data(&new_attr->mbuf, buffer, buffer_length);

  // add the node to our list
  mutex_lock(s_attr_list_lock);
  s_attr_head = (SmartstrapAttributeInternal *)list_prepend(&s_attr_head->list_node,
                                                            &new_attr->list_node);
  mutex_unlock(s_attr_list_lock);
  return true;
}

//! NOTE: the caller must hold s_attr_list_lock
static void prv_queue_deferred_delete(SmartstrapAttributeInternal *attr, bool do_free) {
  mutex_assert_held_by_curr_task(s_attr_list_lock, true);

  if (attr->state != SmartstrapAttributeStateRequestInProgress) {
    // stop the in-progress request
    smartstrap_cancel_send();
  }

  void *buffer = mbuf_get_data(&attr->mbuf);
  // clear out the mbuf just in-case
  attr->mbuf = MBUF_EMPTY;
  if (do_free) {
    applib_free(buffer);
  }
  attr->deferred_delete = true;

  // queue the deferred delete callback on KernelBG
  if (!s_deferred_delete_queued) {
    system_task_add_callback(prv_do_deferred_delete_cb, NULL);
    s_deferred_delete_queued = true;
  }
}

DEFINE_SYSCALL(void, sys_smartstrap_attribute_unregister, SmartstrapAttribute *app_attr) {
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = prv_find_by_buffer((uint8_t *)app_attr);
  if (!attr) {
    mutex_unlock(s_attr_list_lock);
    return;
  }
  prv_queue_deferred_delete(attr, true /* do_free */);
  mutex_unlock(s_attr_list_lock);
}

void smartstrap_attribute_unregister_all(void) {
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr;
  FOREACH_VALID_ATTR(attr) {
    // At this point, the app is closing so there's no point in freeing the buffers, and doing so
    // will crash the watch if the app had crashed (and the heap has already been cleaned up).
    prv_queue_deferred_delete(attr, false /* !do_free */);
  }
  mutex_unlock(s_attr_list_lock);
}

DEFINE_SYSCALL(void, sys_smartstrap_attribute_get_info, SmartstrapAttribute *app_attr,
               uint16_t *service_id, uint16_t *attribute_id, size_t *length) {
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = prv_find_by_buffer((uint8_t *)app_attr);
  mutex_unlock(s_attr_list_lock);
  if (!attr) {
    return;
  }
  if (PRIVILEGE_WAS_ELEVATED) {
    if (service_id) {
      syscall_assert_userspace_buffer(service_id, sizeof(uint16_t));
    }
    if (attribute_id) {
      syscall_assert_userspace_buffer(attribute_id, sizeof(uint16_t));
    }
    if (length) {
      syscall_assert_userspace_buffer(length, sizeof(size_t));
    }
  }
  if (service_id) {
    *service_id = attr->service_id;
  }
  if (attribute_id) {
    *attribute_id = attr->attribute_id;
  }
  if (length) {
    *length = mbuf_get_length(&attr->mbuf);
  }
}

DEFINE_SYSCALL(SmartstrapResult, sys_smartstrap_attribute_do_request, SmartstrapAttribute *app_attr,
               SmartstrapRequestType type, uint16_t timeout_ms, uint32_t write_length) {
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = prv_find_by_buffer((uint8_t *)app_attr);
  mutex_unlock(s_attr_list_lock);
  if (!attr) {
    return SmartstrapResultInvalidArgs;
  } else if (!sys_smartstrap_is_service_connected(attr->service_id)) {
    // go back to idle if we had begun a write
    prv_cancel_transaction(attr);
    return SmartstrapResultServiceUnavailable;
  }

  if (type == SmartstrapRequestTypeBeginWrite) {
    if (attr->write_blocked || !prv_start_transaction(attr, AttributeTransactionBeginWrite)) {
      return SmartstrapResultBusy;
    }
    // clear the write buffer
    memset(mbuf_get_data(&attr->mbuf), 0, mbuf_get_length(&attr->mbuf));
    return SmartstrapResultOk;
  } else if (type == SmartstrapRequestTypeRead) {
    // handle read request
    if (!prv_start_transaction(attr, AttributeTransactionRead)) {
      return SmartstrapResultBusy;
    }
  } else {
    // handle write request
    if (!write_length || (write_length > mbuf_get_length(&attr->mbuf))) {
      prv_cancel_transaction(attr);
      return SmartstrapResultInvalidArgs;
    } else if (!prv_start_transaction(attr, AttributeTransactionEndWrite)) {
      // they didn't call smartstrap_begin_write first
      return SmartstrapResultInvalidArgs;
    }
  }

  attr->write_length = write_length;
  attr->request_type = type;
  attr->timeout_ms = timeout_ms;
  smartstrap_connection_kick_monitor();
  return SmartstrapResultOk;
}

DEFINE_SYSCALL(void, sys_smartstrap_attribute_event_processed, SmartstrapAttribute *app_attr) {
  mutex_lock(s_attr_list_lock);
  SmartstrapAttributeInternal *attr = prv_find_by_buffer((uint8_t *)app_attr);
  mutex_unlock(s_attr_list_lock);
  if (!attr) {
    // the app might have destroyed the attribute
    return;
  }
  // clear the write_blocked flag after the event has been processed for an attribute
  attr->write_blocked = false;
}
