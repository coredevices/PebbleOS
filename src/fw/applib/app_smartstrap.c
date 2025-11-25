/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app_logging.h"
#include "applib/app_smartstrap.h"
#include "applib/app_smartstrap_private.h"
#include "applib/applib_malloc.auto.h"
#include "applib/event_service_client.h"
#include "board/board.h"
#include "process_state/app_state/app_state.h"
#include "services/normal/accessory/smartstrap_attribute.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "system/passert.h"
#include "util/mbuf.h"

#define USE_SMARTSTRAP (CAPABILITY_HAS_ACCESSORY_CONNECTOR && !defined(RECOVERY_FW))

#if USE_SMARTSTRAP
// Event handler
////////////////////////////////////////////////////////////////////////////////

static void prv_app_smartstrap_event(PebbleEvent *e, void *context) {
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  PebbleSmartstrapEvent event = e->smartstrap;
  if (event.type == SmartstrapConnectionEvent) {
    // Drop this event if there's no handler registered
    if (state->handlers.availability_did_change) {
      const SmartstrapServiceId service_id = event.service_id;
      const bool is_available = (event.result == SmartstrapResultOk);
      state->handlers.availability_did_change(service_id, is_available);
    }
  } else {
    // All events other than SmartstrapConnectionEvent contain the attribute pointer
    SmartstrapAttribute *attr = event.attribute;
    PBL_ASSERTN(attr);
    if ((event.type == SmartstrapDataSentEvent) && state->handlers.did_write) {
      state->handlers.did_write(attr, event.result);
    } else if ((event.type == SmartstrapDataReceivedEvent) && state->handlers.did_read) {
      // 'attr' already points to the read buffer, so just need to cast it
      state->handlers.did_read(attr, event.result, (uint8_t *)attr, event.read_length);
    } else if ((event.type == SmartstrapNotifyEvent) && state->handlers.notified) {
      state->handlers.notified(attr);
    }
    sys_smartstrap_attribute_event_processed(attr);
  }
}


// Subscription functions
////////////////////////////////////////////////////////////////////////////////

static bool prv_should_subscribe(void) {
  // We don't need to subscribe until either the app creates an attribute or subscribes an
  // availability_did_change handler.
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  return state->handlers.availability_did_change || state->num_attributes;
}

static void prv_state_init(void) {
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  if (state->is_initialized) {
    return;
  }
  state->event_info = (EventServiceInfo) {
    .type = PEBBLE_SMARTSTRAP_EVENT,
    .handler = prv_app_smartstrap_event
  };
  event_service_client_subscribe(&state->event_info);
  state->timeout_ms = SMARTSTRAP_TIMEOUT_DEFAULT;
  sys_smartstrap_subscribe();
  state->is_initialized = true;
}

static void prv_state_deinit(void) {
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  if (!state->is_initialized) {
    return;
  }
  state->is_initialized = false;
  event_service_client_unsubscribe(&state->event_info);
  sys_smartstrap_unsubscribe();
}
#endif


// Internal APIs
////////////////////////////////////////////////////////////////////////////////

void app_smartstrap_cleanup(void) {
#if USE_SMARTSTRAP
  prv_state_deinit();
#endif
}


// Exported APIs
////////////////////////////////////////////////////////////////////////////////

SmartstrapResult app_smartstrap_subscribe(SmartstrapHandlers handlers) {
#if USE_SMARTSTRAP
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  state->handlers = handlers;
  if (prv_should_subscribe()) {
    prv_state_init();
  }
  return SmartstrapResultOk;
#else
  return SmartstrapResultNotPresent;
#endif
}

void app_smartstrap_unsubscribe(void) {
#if USE_SMARTSTRAP
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  state->handlers = (SmartstrapHandlers) {0};
  if (!prv_should_subscribe()) {
    prv_state_deinit();
  }
#endif
}

void app_smartstrap_set_timeout(uint16_t timeout_ms) {
#if USE_SMARTSTRAP
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  state->timeout_ms = timeout_ms;
#endif
}

SmartstrapAttribute *app_smartstrap_attribute_create(SmartstrapServiceId service_id,
                                                     SmartstrapAttributeId attribute_id,
                                                     size_t buffer_length) {
#if USE_SMARTSTRAP
  if (!buffer_length) {
    return NULL;
  }

  uint8_t *buffer = applib_zalloc(buffer_length);
  if (!buffer) {
    return NULL;
  }

  if (!sys_smartstrap_attribute_register(service_id, attribute_id, buffer, buffer_length)) {
    applib_free(buffer);
    return NULL;
  }
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  state->num_attributes++;
  prv_state_init();
  return (SmartstrapAttribute *)buffer;
#else
  return NULL;
#endif
}

void app_smartstrap_attribute_destroy(SmartstrapAttribute *attr) {
#if USE_SMARTSTRAP
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  state->num_attributes--;
  if (!prv_should_subscribe()) {
    prv_state_deinit();
  }
  sys_smartstrap_attribute_unregister(attr);
#endif
}

bool app_smartstrap_service_is_available(SmartstrapServiceId service_id) {
#if USE_SMARTSTRAP
  return sys_smartstrap_is_service_connected(service_id);
#else
  return false;
#endif
}

SmartstrapServiceId app_smartstrap_attribute_get_service_id(SmartstrapAttribute *attr) {
#if USE_SMARTSTRAP
  SmartstrapServiceId service_id;
  sys_smartstrap_attribute_get_info(attr, &service_id, NULL, NULL);
  return service_id;
#else
  return 0;
#endif
}

SmartstrapAttributeId app_smartstrap_attribute_get_attribute_id(SmartstrapAttribute *attr) {
#if USE_SMARTSTRAP
  SmartstrapServiceId attribute_id;
  sys_smartstrap_attribute_get_info(attr, NULL, &attribute_id, NULL);
  return attribute_id;
#else
  return 0;
#endif
}

SmartstrapResult app_smartstrap_attribute_read(SmartstrapAttribute *attr) {
#if USE_SMARTSTRAP
  if (!attr) {
    return SmartstrapResultInvalidArgs;
  }
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  return sys_smartstrap_attribute_do_request(attr, SmartstrapRequestTypeRead, state->timeout_ms, 0);
#else
  return SmartstrapResultNotPresent;
#endif
}

SmartstrapResult app_smartstrap_attribute_begin_write(SmartstrapAttribute *attr, uint8_t **buffer,
                                                      size_t *buffer_length) {
#if USE_SMARTSTRAP
  if (!attr || !buffer || !buffer_length) {
    return SmartstrapResultInvalidArgs;
  }
  const SmartstrapRequestType type = SmartstrapRequestTypeBeginWrite;
  SmartstrapResult result = sys_smartstrap_attribute_do_request(attr, type, 0, 0);
  if (result == SmartstrapResultOk) {
    *buffer = (uint8_t *)attr;
    sys_smartstrap_attribute_get_info(attr, NULL, NULL, buffer_length);
  }
  return result;
#else
  return SmartstrapResultNotPresent;
#endif
}

SmartstrapResult app_smartstrap_attribute_end_write(SmartstrapAttribute *attr, size_t write_length,
                                                    bool request_read) {
#if USE_SMARTSTRAP
  if (!attr) {
    return SmartstrapResultInvalidArgs;
  }
  SmartstrapRequestType type = request_read ?
                               SmartstrapRequestTypeWriteRead :
                               SmartstrapRequestTypeWrite;
  SmartstrapConnectionState *state = app_state_get_smartstrap_state();
  return sys_smartstrap_attribute_do_request(attr, type, state->timeout_ms, write_length);
#else
  return SmartstrapResultNotPresent;
#endif
}
