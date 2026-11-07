/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>
#include <pbl/pp/protocol.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup pp Pebble protocol
 * @ingroup subsys
 * @brief Messages exchanged with phone apps over any transport.
 *
 * A session carries Pebble protocol messages over a transport, such as PPoGATT over BLE, the
 * serial console or the emulator, and hides the differences between them. Inbound messages are
 * dispatched to @ref pp_endpoint "endpoints"; outbound ones are queued per session with the
 * @ref pp_send "send API" and drained by the transport.
 *
 * There are two kinds of sessions:
 * - The system session talks to the Pebble mobile app. Opening one on a transport closes the
 *   previous one, unless one of them is on a transport that shares the role, like the console.
 *   A hybrid session also carries PebbleKit app traffic.
 * - An app session connects directly to a third party phone app.
 *
 * Sessions may close at any time: a session pointer is only a handle, validated by every call.
 * @{
 */

/** @brief Opaque Pebble protocol session. */
struct pbl_pp_session;

/** @brief Kind of session. */
enum pbl_pp_session_type : int8_t {
  /** Not a valid, or no longer open, session. */
  PBL_PP_SESSION_INVALID = -1,
  /** Session with the Pebble mobile app. */
  PBL_PP_SESSION_SYSTEM = 0,
  /** Session with a third party phone app. */
  PBL_PP_SESSION_APP = 1,
};

/** @brief Session life cycle events, see PBL_PP_SESSION_LISTENER_DEFINE(). */
enum pbl_pp_session_event : uint8_t {
  /** The session opened. */
  PBL_PP_SESSION_OPENED,
  /** The session is closing; it is still valid during the callback. */
  PBL_PP_SESSION_CLOSED,
  /** The phone announced the session's capabilities. */
  PBL_PP_SESSION_CAPABILITIES,
};

/** @brief Session listener. Defined with PBL_PP_SESSION_LISTENER_DEFINE(). */
struct pbl_pp_session_listener {
  /** Called for every event of every session. */
  void (*callback)(struct pbl_pp_session *session, enum pbl_pp_session_event event,
                   void *user_data);
  /** Data passed to @ref callback. */
  void *user_data;
};

/**
 * @brief Define a listener of session events.
 *
 * Listeners are called in the context of the transport that caused the event, which may hold its
 * own locks: they must not block, and should defer work to a task.
 *
 * @param _name Name of the listener, unique across the firmware.
 * @param _callback Function called for every event.
 * @param _user_data Data passed to @p _callback.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_PP_SESSION_LISTENER_DEFINE(_name, _callback, _user_data)                   \
  static const struct pbl_pp_session_listener pbl_pp_session_listener_##_name PBL_USED \
  PBL_UNSORTED_SECTION(pbl_pp_lsn) = {.callback = (_callback), .user_data = (_user_data)}
#else
#define PBL_PP_SESSION_LISTENER_DEFINE(_name, _callback, _user_data)                   \
  static const struct pbl_pp_session_listener pbl_pp_session_listener_##_name PBL_USED \
      PBL_ALIGNED(4) PBL_SECTION(".pbl_pp_session_listeners." #_name) = {              \
        .callback = (_callback),                                                       \
        .user_data = (_user_data),                                                     \
  }
#endif

/**
 * @brief Get the system session.
 *
 * When several system sessions are open, the one on the preferred transport is returned.
 *
 * @return The system session, or NULL if none is open.
 */
struct pbl_pp_session *pbl_pp_session_get_system(void);

/**
 * @brief Get the session serving a watch app.
 *
 * Prefers an app session bound to @p app_uuid, then a hybrid session or an app session whose app
 * is unknown.
 *
 * @param app_uuid UUID of the watch app.
 * @return The session, or NULL if none can serve the app or @p app_uuid is a system UUID.
 */
struct pbl_pp_session *pbl_pp_session_get_app(const Uuid *app_uuid);

/**
 * @brief Get the kind of a session.
 *
 * @param session Session.
 * @return Session type, or @ref PBL_PP_SESSION_INVALID if @p session is not open.
 */
enum pbl_pp_session_type pbl_pp_session_get_type(const struct pbl_pp_session *session);

/**
 * @brief Check whether a session is a system session.
 *
 * @param session Session.
 * @return true if @p session is an open system session.
 */
bool pbl_pp_session_is_system(const struct pbl_pp_session *session);

/**
 * @brief Get the capabilities announced on a session.
 *
 * @param session Session.
 * @return Capability bits, 0 if @p session is not open.
 */
uint32_t pbl_pp_session_get_capabilities(const struct pbl_pp_session *session);

/**
 * @brief Check whether a session announced a capability.
 *
 * @param session Session.
 * @param capability Capability.
 * @return true if @p session is open and announced @p capability.
 */
bool pbl_pp_session_has_capability(const struct pbl_pp_session *session,
                                   enum pbl_pp_capability capability);

/**
 * @brief Ask the transport to reset a session.
 *
 * The transport closes the session and may open a new one.
 *
 * @param session Session.
 */
void pbl_pp_session_reset(struct pbl_pp_session *session);

/**
 * @brief Request a connection response time for a session.
 *
 * Transports without a notion of response time grant every request at once.
 *
 * @param session Session; ignored if not open.
 * @param consumer Consumer making the request.
 * @param state Requested response time state.
 * @param max_period_secs Maximum time to stay in @p state.
 * @param granted Called on KernelMain once @p state is in effect. May be NULL.
 */
void pbl_pp_session_set_responsiveness(struct pbl_pp_session *session,
                                       enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs,
                                       pbl_bt_responsiveness_granted_cb_t granted);

/** @} */
