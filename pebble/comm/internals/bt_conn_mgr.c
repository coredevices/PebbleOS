/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/list.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <comm/ble/gap_le_connect_params.h>
#include <comm/ble/gap_le_connection.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>

//! The Bluetooth Connection Manager is responsible for managing the power
//! state of the active bluetooth connections. Sub-modules using bluetooth are
//! expected to notify this module when they are active or expect inbound data
//! and want to minimize latency. Using this info, the module decides whether
//! the LE or classic connection needs to be bumped out of its lower power
//! state in order to respond more quickly.
//!
//! Note: This module currently only manages the LE connections. In the
//! future, we will add support for handling classic connections as well

typedef struct {
  ListNode list_node;
  uint32_t timeout; // time to stop this request (in uptime ticks)
  enum pbl_bt_response_time_state req_state;
  enum pbl_bt_consumer consumer;
  pbl_bt_responsiveness_granted_cb_t granted_handler;
} ConnectionStateRequest;

typedef struct ConnectionMgrInfo {
  // callback which returns us to a low power state if user of API does not exit
  // a high power state
  struct pbl_wallclock_work watchdog_work;
  // current running state of the connection
  enum pbl_bt_response_time_state curr_requested_state;
  // A list of consumers who have requested changes to latency state != PBL_BT_RESPONSE_TIME_MAX
  ConnectionStateRequest *requests;
} ConnectionMgrInfo;

enum pbl_bt_response_time_state gap_le_connect_params_get_actual_state(GAPLEConnection *connection);

static const char *prv_response_time_state_name(enum pbl_bt_response_time_state state) {
  switch (state) {
    case PBL_BT_RESPONSE_TIME_MAX:
      return "Max";
    case PBL_BT_RESPONSE_TIME_MIDDLE:
      return "Middle";
    case PBL_BT_RESPONSE_TIME_MIN:
      return "Min";
    default:
      return "?";
  }
}

static const char *prv_consumer_name(enum pbl_bt_consumer consumer) {
  static const char *const s_consumer_names[PBL_BT_CONSUMER_NUM] = {
    [PBL_BT_CONSUMER_NONE] = "None",
    [PBL_BT_CONSUMER_APP] = "App",
    [PBL_BT_CONSUMER_LE_PAIRING] = "LePairing",
    [PBL_BT_CONSUMER_LE_SERVICE_DISCOVERY] = "LeServiceDiscovery",
    [PBL_BT_CONSUMER_MUSIC_SERVICE_INDEFINITE] = "MusicServiceIndefinite",
    [PBL_BT_CONSUMER_MUSIC_SERVICE_MOMENTARY] = "MusicServiceMomentary",
    [PBL_BT_CONSUMER_PP_APP_FETCH] = "PpAppFetch",
    [PBL_BT_CONSUMER_PP_APP_MESSAGE] = "PpAppMessage",
    [PBL_BT_CONSUMER_PP_AUDIO_ENDPOINT] = "PpAudioEndpoint",
    [PBL_BT_CONSUMER_PP_GET_BYTES] = "PpGetBytes",
    [PBL_BT_CONSUMER_PP_LOG_DUMP] = "PpLogDump",
    [PBL_BT_CONSUMER_PP_PUT_BYTES] = "PpPutBytes",
    [PBL_BT_CONSUMER_PP_SCREENSHOT] = "PpScreenshot",
    [PBL_BT_CONSUMER_PP_VOICE_ENDPOINT] = "PpVoiceEndpoint",
    [PBL_BT_CONSUMER_PROMPT] = "Prompt",
    [PBL_BT_CONSUMER_TIMELINE_ACTION_MENU] = "TimelineActionMenu",
    [PBL_BT_CONSUMER_PRF] = "PRF",
    [PBL_BT_CONSUMER_PPS_REMOTE_DEVICE] = "PebblePairingServiceRemoteDevice",
    [PBL_BT_CONSUMER_UNIT_TESTS] = "UnitTests",
  };

  if (consumer >= PBL_BT_CONSUMER_NUM || s_consumer_names[consumer] == nullptr) {
    return "?";
  }
  return s_consumer_names[consumer];
}

//! Walks through and finds the lowest latency requested for the given type of
//! connection. Also detects the longest amount of time that interval has been
//! requested. Also gets the consumer that is responsible for the lowest latency + longest timeout
//! combo. These pieces of information are then returned to the caller.
static enum pbl_bt_response_time_state prv_determine_latency_for_connection(
    ConnectionStateRequest *requests, uint16_t *secs_to_wait, enum pbl_bt_consumer *consumer_out) {
  enum pbl_bt_response_time_state state = PBL_BT_RESPONSE_TIME_MAX;
  uint32_t timeout = 0;
  enum pbl_bt_consumer responsible_consumer = PBL_BT_CONSUMER_NONE;

  ConnectionStateRequest *curr_request = requests;
  while (curr_request != nullptr) {
    if (curr_request->req_state > state) {
      // reset our tracker, we have found a higher power mode requested
      timeout = curr_request->timeout;
      state = curr_request->req_state;
      responsible_consumer = curr_request->consumer;
    } else if (curr_request->req_state == state) {
      if (curr_request->timeout > timeout) {
        timeout = curr_request->timeout;
        responsible_consumer = curr_request->consumer;
      }
    }

    curr_request = (ConnectionStateRequest *)list_get_next(&curr_request->list_node);
  }

  if (consumer_out) {
    *consumer_out = responsible_consumer;
  }

  if (secs_to_wait) {
    uint32_t curr_ticks = pbl_uptime_ticks();
    if (curr_ticks < timeout) {
      uint16_t wait_time = (timeout - curr_ticks) / PBL_TICK_HZ;
      *secs_to_wait = MAX(1, wait_time);
    } else {
      *secs_to_wait = 0;
    }
  }

  return state;
}

/*
 * LE connection manager handling for a gateway connection
 */

static void prv_bt_le_gateway_response_latency_watchdog_fn(struct pbl_work *work);

static void prv_granted_kernel_main_cb(void *ctx) {
  pbl_bt_responsiveness_granted_cb_t granted_handler = ctx;
  granted_handler();
}

static void prv_schedule_granted_handler(pbl_bt_responsiveness_granted_cb_t granted_handler) {
  PBL_ASSERTN(granted_handler);
  launcher_task_add_callback(prv_granted_kernel_main_cb, granted_handler);
}

//! extern'd for gap_le_connect_params.c
void conn_mgr_handle_desired_state_granted(GAPLEConnection *hdl,
                                           enum pbl_bt_response_time_state granted_state) {
  bt_lock_assert_held(true);

  ConnectionStateRequest *curr_request = hdl->conn_mgr_info->requests;
  while (curr_request != nullptr) {
    if (curr_request->granted_handler && curr_request->req_state <= granted_state) {
      prv_schedule_granted_handler(curr_request->granted_handler);
      curr_request->granted_handler = nullptr;
    }
    curr_request = (ConnectionStateRequest *)list_get_next(&curr_request->list_node);
  }
}

static void prv_handle_response_latency_for_le_conn(GAPLEConnection *hdl) {
  uint16_t secs_til_max_latency;
  enum pbl_bt_response_time_state state;
  enum pbl_bt_consumer responsible_consumer;
#ifdef CONFIG_RECOVERY_FW
  // We don't care if we burn up some power from PRF and we want FW to update quickly
  secs_til_max_latency = MAX_PERIOD_RUN_FOREVER;
  state = PBL_BT_RESPONSE_TIME_MIN;
  responsible_consumer = 0;
#else
  state = prv_determine_latency_for_connection(hdl->conn_mgr_info->requests, &secs_til_max_latency,
                                               &responsible_consumer);
#endif

  // actually request the mode if it has changed:
  if (hdl->conn_mgr_info->curr_requested_state != state) {
    PBL_LOG_INFO("LE: Requesting state <%s> for %d secs, due to <%s>",
                 prv_response_time_state_name(state), secs_til_max_latency,
                 prv_consumer_name(responsible_consumer));
    gap_le_connect_params_request(hdl, state);
  }

  // remove a watchdog timer if it was already scheduled and schedule a new one
  struct pbl_wallclock_work *watchdog_work = &hdl->conn_mgr_info->watchdog_work;
  if (pbl_wallclock_work_is_running(watchdog_work)) {
    pbl_wallclock_work_stop(watchdog_work);
  }

  // don't start the watchdog timer if we have entered the lowest power mode or
  // if we want to run at the specified rate indefinitely
  if ((state != PBL_BT_RESPONSE_TIME_MAX) && (secs_til_max_latency != MAX_PERIOD_RUN_FOREVER)) {
    // wait an extra second since the first second boundary is somewhere
    // between 0 and 1 seconds from now and we want to make sure the interval
    // we are currently running at actually expires
    pbl_wallclock_work_start(&pbl_sys_workq_high, watchdog_work, 1, secs_til_max_latency + 1);
  }

  hdl->conn_mgr_info->curr_requested_state = state;
}

static bool prv_has_conn_mgr_info(GAPLEConnection *connection, void *data) {
  return connection->conn_mgr_info == data;
}

static void prv_bt_le_gateway_response_latency_watchdog_handler(void *data) {
  bt_lock();
  // The info may have been freed with its connection before this ran.
  GAPLEConnection *hdl = gap_le_connection_find(prv_has_conn_mgr_info, data);
  if (!hdl) {
    goto unlock;
  }

  ConnectionMgrInfo *conn_mgr_info = hdl->conn_mgr_info;

  // if we are executing this cb, we have timed out running at the currently
  // selected state so check and see what consumer timeouts have expired

  ConnectionStateRequest *curr_request = conn_mgr_info->requests;
  uint32_t curr_ticks = pbl_uptime_ticks();
  while (curr_request != nullptr) {
    ConnectionStateRequest *next =
        (ConnectionStateRequest *)list_get_next(&curr_request->list_node);
    if (conn_mgr_info->curr_requested_state == curr_request->req_state) {
      if (curr_ticks >= curr_request->timeout) {
        list_remove(&curr_request->list_node, (ListNode **)&conn_mgr_info->requests, nullptr);
        pbl_free(curr_request);
      }
    }
    curr_request = next;
  }

  // Note: As an optimization, we could track how long we have been in a lower
  // latency state and subtract that from higher latency requests, but most of
  // the time we should be in the maximum latency (low power) state anyway

  // get & set the new state
  prv_handle_response_latency_for_le_conn(hdl);

unlock:
  bt_unlock();
}

static void prv_bt_le_gateway_response_latency_watchdog_fn(struct pbl_work *work) {
  // offload handling onto SysWorkq so we don't stall the high priority work
  // queue trying to get the bt lock
  pbl_sys_work_call(prv_bt_le_gateway_response_latency_watchdog_handler,
                    container_of(work, ConnectionMgrInfo, watchdog_work.work));
}

static bool prv_find_source(ListNode *found_node, void *data) {
  return (((ConnectionStateRequest *)found_node)->consumer ==
          (enum pbl_bt_consumer)(uintptr_t)data);
}

/*
 * Exported APIs
 */

void conn_mgr_set_ble_conn_response_time(GAPLEConnection *hdl, enum pbl_bt_consumer consumer,
                                         enum pbl_bt_response_time_state state,
                                         uint16_t max_period_secs) {
  conn_mgr_set_ble_conn_response_time_ext(hdl, consumer, state, max_period_secs, nullptr);
}

void conn_mgr_set_ble_conn_response_time_ext(GAPLEConnection *hdl, enum pbl_bt_consumer consumer,
                                             enum pbl_bt_response_time_state state,
                                             uint16_t max_period_secs,
                                             pbl_bt_responsiveness_granted_cb_t granted_handler) {
  ConnectionMgrInfo *conn_mgr_info;
  if (!hdl || !((conn_mgr_info = hdl->conn_mgr_info))) {
    PBL_LOG_ERR("GAP Handle not properly initialized");
    return;
  }

  bt_lock();
  // remove the watchdog timer if it was already scheduled since we are
  // going to recompute
  struct pbl_wallclock_work *watchdog_work = &hdl->conn_mgr_info->watchdog_work;
  if (pbl_wallclock_work_is_running(watchdog_work)) {
    pbl_wallclock_work_stop(watchdog_work);
  }

  ConnectionStateRequest *consumer_request = (ConnectionStateRequest *)list_find(
      (ListNode *)conn_mgr_info->requests, prv_find_source, (void *)consumer);

  bool is_already_granted = (gap_le_connect_params_get_actual_state(hdl) >= state);

  if (consumer_request == nullptr) {
    if (state == PBL_BT_RESPONSE_TIME_MAX) {
      // No changes: there was no previous node and the new state is the default "low power" one.
      goto handle_current_state;
    }

    // create node
    consumer_request = pbl_malloc_check(sizeof(ConnectionStateRequest));
    list_init(&consumer_request->list_node);
    conn_mgr_info->requests = (ConnectionStateRequest *)list_prepend(
        &conn_mgr_info->requests->list_node, &consumer_request->list_node);
  }

  // If the consumer requests to go back to low power (PBL_BT_RESPONSE_TIME_MAX), wait a little
  // longer before actually going back. This prevents rapid back-n-forths between low power and fast
  // modes, that can happen especially in a chain of operations, for example, the resource & bin
  // put-bytes sessions to install an app.
  if (state == PBL_BT_RESPONSE_TIME_MAX) {
    // Keep the existing node in the list for the duration of our "activity timeout". It will be
    // cleaned up automatically by the watchdog timer.
    max_period_secs = BT_CONN_MGR_INACTIVITY_TIMEOUT_SECS;
    state = consumer_request->req_state;
  }

  // populate node with new info. If it was previously set we override it
  consumer_request->timeout = pbl_uptime_ticks() + max_period_secs * PBL_TICK_HZ;
  consumer_request->req_state = state;
  consumer_request->consumer = consumer;
  consumer_request->granted_handler = is_already_granted ? nullptr : granted_handler;

handle_current_state:

  if (is_already_granted && granted_handler) {
    prv_schedule_granted_handler(granted_handler);
  }
  prv_handle_response_latency_for_le_conn(hdl);

  bt_unlock();
}

//! expects that the bt lock is held
ConnectionMgrInfo *bt_conn_mgr_info_init(void) {
  ConnectionMgrInfo *newinfo = pbl_malloc_check(sizeof(ConnectionMgrInfo));
  *newinfo = (ConnectionMgrInfo){
    .curr_requested_state = PBL_BT_RESPONSE_TIME_MAX,
  };
  pbl_wallclock_work_init(&newinfo->watchdog_work, prv_bt_le_gateway_response_latency_watchdog_fn);

  return newinfo;
}

//! expects that the bt_lock is held
void bt_conn_mgr_info_deinit(ConnectionMgrInfo **info) {
  // A running function only passes the info's address on, checked under bt_lock.
  pbl_wallclock_work_stop(&(*info)->watchdog_work);

  ListNode *curr_request = (ListNode *)(*info)->requests;
  while (curr_request != nullptr) {
    ListNode *temp = list_get_next(curr_request);
    list_remove(curr_request, nullptr, nullptr);
    pbl_free(curr_request);
    curr_request = temp;
  }

  pbl_free(*info);
  *info = nullptr;
}

enum pbl_bt_response_time_state conn_mgr_get_latency_for_le_connection(GAPLEConnection *hdl,
                                                                       uint16_t *secs_to_wait) {
  bt_lock_assert_held(true);
  return prv_determine_latency_for_connection(hdl->conn_mgr_info->requests, secs_to_wait, nullptr);
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_le_mode(const struct pbl_shell *sh, size_t argc, char **argv) {
  unsigned long state;

  if (pbl_shell_strtoul(argv[1], &state) != 0 || state >= PBL_BT_RESPONSE_TIME_NUM) {
    pbl_shell_error(sh, "invalid mode '%s'", argv[1]);
    return -EINVAL;
  }

  conn_mgr_set_ble_conn_response_time(gap_le_connection_any(), PBL_BT_CONSUMER_PROMPT,
                                      (enum pbl_bt_response_time_state)state,
                                      MAX_PERIOD_RUN_FOREVER);
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, le_mode, nullptr, "Set the LE response time <0-2>", prv_cmd_le_mode, 2,
                     0);
#endif
