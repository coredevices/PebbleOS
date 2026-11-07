/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pp_private.h"

#include <errno.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>

#include <kernel/event_loop.h>
#include <kernel/events.h>

PBL_LOG_MODULE_DEFINE(pp, CONFIG_PP_LOG_LEVEL);

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_pp_session_listener __pbl_pp_session_listeners_start[];
extern const struct pbl_pp_session_listener __pbl_pp_session_listeners_end[];
#else
extern const struct pbl_pp_session_listener
    __pbl_pp_session_listeners_start[] PBL_UNSORTED_SECTION_START(pbl_pp_lsn);
extern const struct pbl_pp_session_listener
    __pbl_pp_session_listeners_end[] PBL_UNSORTED_SECTION_END(pbl_pp_lsn);
#endif

static PBL_MUTEX_DEFINE(s_lock);

static struct pbl_pp_session *s_sessions;

void pbl_pp__lock(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
}

void pbl_pp__unlock(void) {
  pbl_mutex_unlock(&s_lock);
}

void pbl_pp__assert_locked(void) {
  pbl_mutex_assert_held(&s_lock, true);
}

bool pbl_pp__session_is_open(const struct pbl_pp_session *session) {
  return (session != nullptr) && list_contains((ListNode *)s_sessions, &session->node);
}

static bool prv_is_system(const struct pbl_pp_session *session) {
  return session->destination != PBL_PP_DESTINATION_APP;
}

static const struct pbl_pp_transport *prv_get_transport(const struct pbl_pp_session *session) {
  const struct pbl_pp_transport *transport = nullptr;

  pbl_pp__lock();
  if (pbl_pp__session_is_open(session)) {
    transport = session->transport;
  }
  pbl_pp__unlock();

  return transport;
}

static void prv_notify(struct pbl_pp_session *session, enum pbl_pp_session_event event) {
  for (const struct pbl_pp_session_listener *l = __pbl_pp_session_listeners_start;
       l < __pbl_pp_session_listeners_end; l++) {
    l->callback(session, event, l->user_data);
  }
}

static void prv_put_event(bool is_open, bool is_system) {
  PebbleEvent event = {
    .type = PEBBLE_COMM_SESSION_EVENT,
    .bluetooth.comm_session_event.is_open = is_open,
    .bluetooth.comm_session_event.is_system = is_system,
  };

  event_put(&event);
}

static void prv_put_events(const struct pbl_pp_session *session, bool is_open) {
  prv_put_event(is_open, prv_is_system(session));
  if (session->destination == PBL_PP_DESTINATION_HYBRID) {
    prv_put_event(is_open, false);
  }
}

static void prv_log_event(const struct pbl_pp_session *session, bool is_open) {
  static const char destinations[] = {
    [PBL_PP_DESTINATION_SYSTEM] = 'S',
    [PBL_PP_DESTINATION_APP] = 'A',
    [PBL_PP_DESTINATION_HYBRID] = 'H',
  };
  char uuid_string[UUID_STRING_BUFFER_LENGTH];

  uuid_to_string(&session->app_uuid, uuid_string);
  PBL_LOG_INFO("Session event: is_open=%d, destination=%c, app_uuid=%s, transport=%s", is_open,
               destinations[session->destination], uuid_string, session->transport->name);
}

static int prv_close_system_session(void) {
  struct pbl_pp_session *existing = nullptr;

  pbl_pp__lock();
  for (struct pbl_pp_session *s = s_sessions; s != nullptr;
       s = (struct pbl_pp_session *)s->node.next) {
    if (prv_is_system(s) && !s->transport->shared) {
      existing = s;
      break;
    }
  }
  const struct pbl_pp_transport *transport = (existing != nullptr) ? existing->transport : nullptr;
  pbl_pp__unlock();

  if (existing == nullptr) {
    return 0;
  }
  if (transport->close == nullptr) {
    PBL_LOG_ERR("System session on %s cannot be closed", transport->name);
    return -EBUSY;
  }

  PBL_LOG_INFO("Closing the system session on %s", transport->name);
  transport->close(existing);

  return 0;
}

struct pbl_pp_session *pbl_pp_session_open(const struct pbl_pp_transport *transport,
                                           enum pbl_pp_destination destination,
                                           const Uuid *app_uuid) {
  struct pbl_pp_session *session;

  if (destination != PBL_PP_DESTINATION_APP && !transport->shared &&
      prv_close_system_session() != 0) {
    return nullptr;
  }

  session = pbl_malloc(sizeof(*session));
  if (session == nullptr) {
    PBL_LOG_ERR("Out of memory for a session");
    return nullptr;
  }

  *session = (struct pbl_pp_session){
    .transport = transport,
    .app_uuid = (app_uuid != nullptr) ? *app_uuid : UUID_INVALID,
    .destination = destination,
  };

  pbl_pp__lock();
  s_sessions = (struct pbl_pp_session *)list_prepend((ListNode *)s_sessions, &session->node);
  pbl_pp__unlock();

  prv_log_event(session, true);
  pbl_pp__version_request(session);
  prv_notify(session, PBL_PP_SESSION_OPENED);
  prv_put_events(session, true);

  return session;
}

void pbl_pp_session_close(struct pbl_pp_session *session) {
  pbl_pp__lock();
  PBL_ASSERTN(pbl_pp__session_is_open(session));
  pbl_pp__unlock();

  prv_log_event(session, false);
  prv_notify(session, PBL_PP_SESSION_CLOSED);
  prv_put_events(session, false);

  pbl_pp__receive_drop(session);

  pbl_pp__lock();
  pbl_pp__send_queue_drop(session);
  list_remove(&session->node, (ListNode **)&s_sessions, nullptr);
  pbl_pp__unlock();

  pbl_free(session);
}

struct pbl_pp_session *pbl_pp_session_get_system(void) {
  struct pbl_pp_session *best = nullptr;

  pbl_pp__lock();
  for (struct pbl_pp_session *s = s_sessions; s != nullptr;
       s = (struct pbl_pp_session *)s->node.next) {
    if (prv_is_system(s) &&
        (best == nullptr || s->transport->priority > best->transport->priority)) {
      best = s;
    }
  }
  pbl_pp__unlock();

  return best;
}

struct pbl_pp_session *pbl_pp_session_get_app(const Uuid *app_uuid) {
  struct pbl_pp_session *match = nullptr;
  struct pbl_pp_session *fallback = nullptr;

  if (app_uuid == nullptr || uuid_is_system(app_uuid) || uuid_is_invalid(app_uuid)) {
    return nullptr;
  }

  pbl_pp__lock();
  for (struct pbl_pp_session *s = s_sessions; s != nullptr;
       s = (struct pbl_pp_session *)s->node.next) {
    if (uuid_equal(&s->app_uuid, app_uuid)) {
      match = s;
      break;
    }
    if (s->destination == PBL_PP_DESTINATION_HYBRID ||
        (s->destination == PBL_PP_DESTINATION_APP && uuid_is_invalid(&s->app_uuid))) {
      fallback = s;
    }
  }
  pbl_pp__unlock();

  return (match != nullptr) ? match : fallback;
}

enum pbl_pp_session_type pbl_pp_session_get_type(const struct pbl_pp_session *session) {
  enum pbl_pp_session_type type = PBL_PP_SESSION_INVALID;

  pbl_pp__lock();
  if (pbl_pp__session_is_open(session)) {
    type = prv_is_system(session) ? PBL_PP_SESSION_SYSTEM : PBL_PP_SESSION_APP;
  }
  pbl_pp__unlock();

  return type;
}

bool pbl_pp_session_is_system(const struct pbl_pp_session *session) {
  return pbl_pp_session_get_type(session) == PBL_PP_SESSION_SYSTEM;
}

uint32_t pbl_pp_session_get_capabilities(const struct pbl_pp_session *session) {
  uint32_t capabilities = 0U;

  pbl_pp__lock();
  if (pbl_pp__session_is_open(session)) {
    capabilities = session->capabilities;
  }
  pbl_pp__unlock();

  return capabilities;
}

bool pbl_pp_session_has_capability(const struct pbl_pp_session *session,
                                   enum pbl_pp_capability capability) {
  return (pbl_pp_session_get_capabilities(session) & capability) != 0U;
}

void pbl_pp_session_set_capabilities(struct pbl_pp_session *session, uint32_t capabilities) {
  bool is_open;

  pbl_pp__lock();
  is_open = pbl_pp__session_is_open(session);
  if (is_open) {
    session->capabilities = capabilities;
  }
  pbl_pp__unlock();

  if (is_open) {
    prv_notify(session, PBL_PP_SESSION_CAPABILITIES);
  }
}

void pbl_pp_session_reset(struct pbl_pp_session *session) {
  const struct pbl_pp_transport *transport = prv_get_transport(session);

  if (transport == nullptr) {
    PBL_LOG_WRN("Session already closed");
    return;
  }
  if (transport->reset == nullptr) {
    PBL_LOG_WRN("%s cannot reset sessions", transport->name);
    return;
  }

  transport->reset(session);
}

static void prv_granted_cb(void *data) {
  pbl_bt_responsiveness_granted_cb_t granted = data;

  granted();
}

void pbl_pp_session_set_responsiveness(struct pbl_pp_session *session,
                                       enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs,
                                       pbl_bt_responsiveness_granted_cb_t granted) {
  const struct pbl_pp_transport *transport = prv_get_transport(session);

  if (transport == nullptr) {
    return;
  }

  if (transport->set_responsiveness != nullptr) {
    transport->set_responsiveness(session, consumer, state, max_period_secs, granted);
  } else if (granted != nullptr) {
    launcher_task_add_callback(prv_granted_cb, granted);
  }
}

static void prv_send_next_cb(void *data) {
  struct pbl_pp_session *session = data;
  const struct pbl_pp_transport *transport = nullptr;

  pbl_pp__lock();
  if (pbl_pp__session_is_open(session)) {
    session->send_next_pending = false;
    transport = session->transport;
  }
  pbl_pp__unlock();

  if (transport != nullptr) {
    transport->send_next(session);
  }
}

void pbl_pp_session_send_next(struct pbl_pp_session *session) {
  bool schedule;

  pbl_pp__lock();
  schedule = pbl_pp__session_is_open(session) && !session->send_next_pending;
  if (schedule) {
    session->send_next_pending = true;
  }
  pbl_pp__unlock();

  if (schedule) {
    launcher_task_add_callback(prv_send_next_cb, session);
  }
}

void pbl_pp__send_next_now(struct pbl_pp_session *session) {
  const struct pbl_pp_transport *transport = prv_get_transport(session);

  if (transport != nullptr) {
    transport->send_next(session);
  }
}

bool pbl_pp__in_send_task(void) {
  return launcher_task_is_current_task();
}

#if UNITTEST
bool pbl_pp__send_next_is_pending(const struct pbl_pp_session *session) {
  return session->send_next_pending;
}
#endif
