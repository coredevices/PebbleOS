/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ppogatt_internal.h"

#include <errno.h>
#include <inttypes.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/ppog_reversed.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/main.h>
#include <pbl/pp/send.h>
#include <pbl/pp/transport.h>
#include <pbl/pp/transports/gatt.h>
#include <pbl/util/list.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <host/ble_att.h>
#include <host/ble_gap.h>
#include <host/ble_hs.h>

//! How many PPoGATT services of the phone are looked for: the Pebble app's, and those of apps.
#define FORWARD_SERVICES_MAX 4

static const ble_uuid128_t s_forward_svc_uuid =
    PBL_BT_PEBBLE_UUID128_INIT(PBL_BT_PEBBLE_PPOGATT_SERVICE_UUID_32BIT);
static const ble_uuid128_t s_forward_data_uuid =
    PBL_BT_PEBBLE_UUID128_INIT(PBL_BT_PEBBLE_PPOGATT_DATA_CHARACTERISTIC_UUID_32BIT);
static const ble_uuid128_t s_forward_meta_uuid =
    PBL_BT_PEBBLE_UUID128_INIT(PBL_BT_PEBBLE_PPOGATT_META_CHARACTERISTIC_UUID_32BIT);

static const ble_uuid_t *const s_forward_chr_uuids[PBL_PP_GATT_CHARACTERISTIC_NUM] = {
  [PBL_PP_GATT_CHARACTERISTIC_DATA] = &s_forward_data_uuid.u,
  [PBL_PP_GATT_CHARACTERISTIC_META] = &s_forward_meta_uuid.u,
};

//! The clients of the phone's PPoGATT services, one per instance of the service.
static struct pbl_bt_gattc_chr s_forward_chrs[FORWARD_SERVICES_MAX][PBL_PP_GATT_CHARACTERISTIC_NUM];
static struct pbl_bt_gattc_service s_forward_services[FORWARD_SERVICES_MAX];

typedef enum {
  StateDisconnectedReadingMeta,
  StateDisconnectedAwaitingMetaRetry,
  StateDisconnectedSubscribingData,
  //! Reversed role only: waiting for the phone to send the initial ResetRequest.
  StateConnectedClosedAwaitingResetRequest,
  StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset,
  StateConnectedClosedAwaitingResetCompleteSelfInitiatedResetStalled,
  StateConnectedClosedAwaitingResetCompleteRemoteInitiatedReset,
  StateConnectedOpen,
} State;

typedef enum {
  DeleteReason_DuplicateServer = 100,
  DeleteReason_ServiceRemoved,
  DeleteReason_InvalidateAllReferences,
  DeleteReason_ResetSelfInitiated,
  DeleteReason_ResetRemoteInitiated,
  DeleteReason_CloseCalled,
  DeleteReason_DestroyCalled,
  DeleteReason_SubscribeFailure,
  DeleteReason_MetaDataReadFailure,
  DeleteReason_MetaDataInvalid,
  DeleteReason_CouldntOpenSession,
  DeleteReasonCount,
} DeleteReason;

typedef enum {
  AckTimeoutState_Inactive = 0,
  AckTimeoutState_Active = 1,
  AckTimeoutState_TimedOut = AckTimeoutState_Active + PPOGATT_TIMEOUT_TICKS,
} AckTimeoutState;

typedef struct PPoGATTClient {
  ListNode node;
  State state;
  uint8_t version;
  PPoGATTRole role;

  // TODO: Save some memory and point to app metadata instead?
  Uuid app_uuid;

  //! State for the forward role (phone hosts the GATT service).
  struct {
    //! Which of the phone's PPoGATT services, the index in s_forward_services.
    uint8_t instance;
  } fwd;

  //! State for the reversed role (watch hosts the GATT service).
  struct {
    uint16_t conn_handle;
  } rev;

  //! Stuffs that deals with inbound data
  struct {
    uint8_t next_expected_data_sn;
  } in;

  //! Stuffs that deals with outbound data
  struct {
    union {
      PPoGATTPacketHeader reset_packet_to_send;
      //! Set to 0 if there is no reset packet to send
      uint8_t reset_packet_byte;
    };
    union {
      PPoGATTPacketHeader ack_packet_to_send;
      //! Set to 0 if there is no ack packet to send
      uint8_t ack_packet_byte;
    };

    uint16_t payload_sizes[PPOGATT_SN_MOD_DIV];
    uint8_t tx_window_size;
    uint8_t rx_window_size;

    AckTimeoutState ack_timeout_state;

    //! Number of consecutive timeouts so far
    uint8_t timeouts_counter;

    uint8_t next_expected_ack_sn;
    uint8_t next_data_sn;

    bool send_rx_ack_now;             //! True if we want to flush the Ack immediately!
    uint8_t outstanding_rx_ack_count; //! Count of how many data packets we have yet to Ack
  } out;

  //! Number of consecutive resets so far
  uint8_t resets_counter;

  //! Number of meta read retry attempts
  uint8_t meta_read_retries;

  bool disconnect_requested; //! True if the client requested a disconnect

  //! Guards prv_send_next_packets against re-entry while the lock is dropped.
  bool send_in_progress;

  //! Number of consecutive send retries for lack of BT stack buffers; 0 when
  //! not stalled.
  uint16_t send_retry_count;

  struct pbl_delayed_work rx_ack_work; //! Ensures Acks for data are dispatched regularly

  struct pbl_delayed_work meta_read_retry_work;

  //! Retries sending after the BT stack ran out of buffers
  //! (NimBLE has no "buffers freed" event to wait for)
  struct pbl_delayed_work send_retry_work;

  //! Whether the PPoGATT server transports "System", "App" or "Hybrid" PP sessions.
  enum pbl_pp_destination destination;

  //! Session of the client, opened and closed by it.
  struct pbl_pp_session *session;

  //! Tick count when this client was created, for measuring handshake duration
  uint64_t created_ticks;
} PPoGATTClient;

// -------------------------------------------------------------------------------------------------
// Static variables

//! Protects the clients. Not held while calling into NimBLE.
static PBL_MUTEX_DEFINE(s_lock);

static PPoGATTClient *s_ppogatt_head;

static struct pbl_wallclock_work s_ack_work;

static uint8_t s_disconnect_counter;

//! Caps rediscovery on stale meta handles to once per BLE connection.
static bool s_rediscovery_requested_this_connection;

//! The connection the phone's PPoGATT services are looked for on.
static uint16_t s_forward_conn_handle;

//! Tells what the service clients report for a search that was given up from the current one.
static uint32_t s_forward_generation;

//! The connection on which the phone started a reversed PPoG handshake, or
//! BLE_HS_CONN_HANDLE_NONE. Forward PPoG yields to reversed on this connection.
static uint16_t s_reversed_active_conn_handle = BLE_HS_CONN_HANDLE_NONE;

// -------------------------------------------------------------------------------------------------
// Function Prototypes

static void prv_send_next_packets(PPoGATTClient *client);
static void prv_start_reset(PPoGATTClient *client);
static const struct pbl_pp_transport s_transport;
static void prv_request_meta_rediscovery(PPoGATTClient *client);
static void prv_reversed_take_over(PPoGATTClient *client);
static void prv_rx_ack_fn(struct pbl_work *work);
static void prv_meta_read_retry_fn(struct pbl_work *work);
static void prv_send_retry_fn(struct pbl_work *work);

static void prv_forward_init(void);
static void prv_forward_discover(uint8_t instance);
static void prv_forward_restart(void);
static void prv_forward_stop(void);

// -------------------------------------------------------------------------------------------------
void pbl_pp_gatt_reset_disconnect_counter(void) {
  s_disconnect_counter = 0;
}

static bool prv_client_supports_enhanced_throughput_features(const PPoGATTClient *client) {
  // In PPoGATT V1, two features were added to allow for enhanced throughput:
  // 1) Negotiable RX/TX in-flight windows - This let's the phone put more data out over the
  //                                         air and not block waiting for an Ack
  // 2) Coalesced ACKing - Since 1) makes the window size larger, it's beneficial to flush Acks less
  //                       frequently. This reduces the strain on the BT controller scheduler and
  //                       frees up more slots for outbound data packets. As long as we send an Ack
  //                       before the in-flight window fills, the phone can keep pushing data.
  //                       If very little data is in flight, flushing Acks periodically will have no
  //                       impact on throughput
  return (client->version >= 1);
}

// -------------------------------------------------------------------------------------------------

static void prv_lock(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
}

static void prv_unlock(void) {
  pbl_mutex_unlock(&s_lock);
}

static uint16_t prv_get_conn_handle(const PPoGATTClient *client) {
  return (client->role == PPoGATTRoleReversed) ? client->rev.conn_handle : s_forward_conn_handle;
}

// -------------------------------------------------------------------------------------------------

static void prv_send_next_packets_async(PPoGATTClient *client) {
  // The session skips scheduling if a send_next call is already pending.
  pbl_pp_session_send_next(client->session);
}

// -------------------------------------------------------------------------------------------------

static uint32_t prv_sn_distance(uint8_t sn_begin_incl, uint32_t sn_end_excl) {
  return ((uint32_t)PPOGATT_SN_MOD_DIV + sn_end_excl - sn_begin_incl) % PPOGATT_SN_MOD_DIV;
}

//! @return Number of packets in flight, *excluding* packets that are pending retransmission.
static uint32_t prv_num_packets_in_flight(const PPoGATTClient *client) {
  return prv_sn_distance(client->out.next_expected_ack_sn, client->out.next_data_sn);
}

static uint32_t prv_next_sn(uint32_t current_sn) {
  return (current_sn + 1) % PPOGATT_SN_MOD_DIV;
}

static uint32_t prv_prev_sn(uint32_t sn) {
  return ((PPOGATT_SN_MOD_DIV + sn - 1) % PPOGATT_SN_MOD_DIV);
}

// -------------------------------------------------------------------------------------------------

static uint16_t prv_get_payload_size_for_sn(const PPoGATTClient *client, uint32_t sn) {
  return client->out.payload_sizes[sn];
}

static bool prv_is_packet_with_sn_awaiting_ack(const PPoGATTClient *client, uint32_t sn) {
  return (prv_get_payload_size_for_sn(client, sn) != 0);
}

static uint16_t prv_total_num_bytes_awaiting_ack_up_to(const PPoGATTClient *client,
                                                       uint32_t sn_end_excl) {
  uint16_t num_bytes = 0;
  for (uint32_t sn = client->out.next_expected_ack_sn; sn != sn_end_excl; sn = prv_next_sn(sn)) {
    num_bytes += prv_get_payload_size_for_sn(client, sn);
  }
  return num_bytes;
}

static uint16_t prv_total_num_bytes_awaiting_ack(const PPoGATTClient *client) {
  return prv_total_num_bytes_awaiting_ack_up_to(client, client->out.next_data_sn);
}

static void prv_set_payload_size_for_sn(PPoGATTClient *client, uint32_t sn, uint16_t payload_size) {
  client->out.payload_sizes[sn] = payload_size;
}

static void prv_clear_payload_sizes_up_to(PPoGATTClient *client, uint32_t sn_end_excl) {
  for (uint32_t sn = client->out.next_expected_ack_sn; sn != sn_end_excl; sn = prv_next_sn(sn)) {
    prv_set_payload_size_for_sn(client, sn, 0);
  }
}

// -------------------------------------------------------------------------------------------------
// Ack Time-out related things.

static void prv_ack_tick_fn(struct pbl_work *work);

//! Only tick while some client waits for an ack, so an idle link does not wake the system.
static void prv_update_ack_timer(void) {
  bool waiting = false;
  for (PPoGATTClient *client = s_ppogatt_head; client;
       client = (PPoGATTClient *)client->node.next) {
    if (client->out.ack_timeout_state != AckTimeoutState_Inactive) {
      waiting = true;
      break;
    }
  }

  if (!waiting) {
    if (pbl_wallclock_work_is_running(&s_ack_work)) {
      pbl_wallclock_work_stop(&s_ack_work);
    }
  } else if (!pbl_wallclock_work_is_running(&s_ack_work)) {
    pbl_wallclock_work_init(&s_ack_work, prv_ack_tick_fn);
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_ack_work, 1,
                             PPOGATT_TIMEOUT_TICK_INTERVAL_SECS);
  }
}

static void prv_reset_ack_timeout(PPoGATTClient *client) {
  client->out.ack_timeout_state = AckTimeoutState_Active;
  prv_update_ack_timer();
}

static void prv_roll_back(PPoGATTClient *client, uint32_t sn) {
  if (++client->out.timeouts_counter >= PPOGATT_TIMEOUT_COUNT_MAX) {
    PBL_LOG_ERR("Resetting because max timeouts reached...");
    prv_start_reset(client);
    return;
  }

  PBL_LOG_WRN("Rolling back from (%u, %u) to %" PRIu32, client->out.next_data_sn,
              client->out.next_expected_ack_sn, sn);

  // Go back and send again:
  // No need to worry about the timeouts of these packets hitting, because prv_check_timeouts uses
  // next_data_sn and next_expected_ack_sn to determine which packets can time-out.
  client->out.next_data_sn = sn;
  client->out.next_expected_ack_sn = sn;
  prv_reset_ack_timeout(client);

  // Don't send from Timer task
  prv_send_next_packets_async(client);
}

static bool prv_has_timeout(const PPoGATTClient *client) {
  return (client->out.ack_timeout_state != AckTimeoutState_Inactive &&
          client->out.ack_timeout_state >= AckTimeoutState_TimedOut);
}

static void prv_increment_timeout_counter_if_necessary(PPoGATTClient *client) {
  if (client->out.ack_timeout_state >= AckTimeoutState_Active) {
    client->out.ack_timeout_state++;
  }
}

static void prv_check_timeouts(PPoGATTClient *client) {
  if (client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset ||
      client->state == StateConnectedClosedAwaitingResetCompleteRemoteInitiatedReset) {
    if (prv_has_timeout(client)) {
      // We've timed out waiting for a reset to be completed, start over:
      PBL_LOG_WRN("Timed out waiting for Reset Complete, Resetting again...");
      prv_start_reset(client);
    }
    return;
  } else if (client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedResetStalled) {
    return;
  }

  uint8_t sn = client->out.next_expected_ack_sn;
  if (prv_has_timeout(client)) {
    prv_roll_back(client, sn);
    // Return, because all packets after the timed-out one have been "rolled back" now,
    // no point in continuing.
    return;
  }
}

static void prv_ack_timeout_main_cb(void *unused) {
  prv_lock();
  {
    PPoGATTClient *client = s_ppogatt_head;
    while (client) {
      prv_increment_timeout_counter_if_necessary(client);
      prv_check_timeouts(client);
      client = (PPoGATTClient *)client->node.next;
    }
    prv_update_ack_timer();
  }
  prv_unlock();
}

//! Never block on the lock on the high priority work queue (it stalls all other work there);
//! bounce to the main thread, where the rest of the PPoG state machine runs.
static void prv_ack_tick_fn(struct pbl_work *work) {
  pbl_pp_main_call(prv_ack_timeout_main_cb, nullptr);
}

// -------------------------------------------------------------------------------------------------

static PPoGATTClient *prv_create_client(void) {
  PPoGATTClient *client = pbl_malloc(sizeof(PPoGATTClient));
  if (!client) {
    return nullptr;
  }
  *client = (PPoGATTClient){};
  client->app_uuid = UUID_INVALID;
  pbl_delayed_work_init(&client->rx_ack_work, prv_rx_ack_fn);
  pbl_delayed_work_init(&client->meta_read_retry_work, prv_meta_read_retry_fn);
  pbl_delayed_work_init(&client->send_retry_work, prv_send_retry_fn);
  client->created_ticks = pbl_uptime_ticks();
  s_ppogatt_head = (PPoGATTClient *)list_prepend((ListNode *)s_ppogatt_head, &client->node);
  return client;
}

// -------------------------------------------------------------------------------------------------

static void prv_delete_client(PPoGATTClient *client, bool is_disconnected, DeleteReason reason) {
  const uint32_t elapsed_ms = (pbl_uptime_ticks() - client->created_ticks) * 1000 / PBL_TICK_HZ;
  PBL_LOG_INFO("PPoGATT client deleted: role=%u state=%u reason=%u disconnected=%u after %" PRIu32
               "ms",
               client->role, client->state, reason, is_disconnected, elapsed_ms);
  // Unsubscribe from the phone's Data characteristic (forward role only):
  if (client->role == PPoGATTRoleForward && client->state > StateDisconnectedSubscribingData &&
      !is_disconnected) {
    struct pbl_bt_gattc_service *svc = &s_forward_services[client->fwd.instance];
    // Release the lock before calling into NimBLE.
    prv_unlock();
    (void)pbl_bt_gattc_service_subscribe(svc, PBL_PP_GATT_CHARACTERISTIC_DATA, false);
    prv_lock();
  }

  if (client->state == StateConnectedOpen) {
    pbl_pp_session_close(client->session);
  }

  if (client->role == PPoGATTRoleReversed &&
      s_reversed_active_conn_handle == client->rev.conn_handle) {
    s_reversed_active_conn_handle = BLE_HS_CONN_HANDLE_NONE;
  }

  list_remove(&client->node, (ListNode **)&s_ppogatt_head, nullptr);
  // No need to wait for a running function: they never dereference the client.
  pbl_delayed_work_cancel(&client->rx_ack_work);
  pbl_delayed_work_cancel(&client->meta_read_retry_work);
  pbl_delayed_work_cancel(&client->send_retry_work);
  pbl_free(client);

  prv_update_ack_timer();
}

// -------------------------------------------------------------------------------------------------

static bool prv_forward_instance_filter_callback(ListNode *found_node, void *data) {
  const PPoGATTClient *client = (const PPoGATTClient *)found_node;
  return (client->role == PPoGATTRoleForward && client->fwd.instance == (uintptr_t)data);
}

static PPoGATTClient *prv_find_forward_client_with_instance(uint8_t instance) {
  return (PPoGATTClient *)list_find((ListNode *)s_ppogatt_head,
                                    prv_forward_instance_filter_callback,
                                    (void *)(uintptr_t)instance);
}

// -------------------------------------------------------------------------------------------------

static bool prv_uuid_filter_callback(ListNode *found_node, void *data) {
  const PPoGATTClient *client = (const PPoGATTClient *)found_node;
  const Uuid *uuid = (const Uuid *)data;
  return uuid_equal(&client->app_uuid, uuid);
}

static PPoGATTClient *prv_find_client_with_uuid(const Uuid *uuid) {
  return (PPoGATTClient *)list_find((ListNode *)s_ppogatt_head, prv_uuid_filter_callback,
                                    (void *)uuid);
}

// -------------------------------------------------------------------------------------------------

static bool prv_reversed_conn_filter_callback(ListNode *found_node, void *data) {
  const PPoGATTClient *client = (const PPoGATTClient *)found_node;
  const uint16_t conn_handle = (uint16_t)(uintptr_t)data;
  return (client->role == PPoGATTRoleReversed && client->rev.conn_handle == conn_handle);
}

static PPoGATTClient *prv_find_reversed_client_for_conn(uint16_t conn_handle) {
  return (PPoGATTClient *)list_find((ListNode *)s_ppogatt_head, prv_reversed_conn_filter_callback,
                                    (void *)(uintptr_t)conn_handle);
}

static bool prv_forward_connection_filter_callback(ListNode *found_node, void *data) {
  const PPoGATTClient *client = (const PPoGATTClient *)found_node;
  return (client->role == PPoGATTRoleForward && (uint16_t)(uintptr_t)data == s_forward_conn_handle);
}

static PPoGATTClient *prv_find_forward_client_for_conn(uint16_t conn_handle) {
  return (PPoGATTClient *)list_find((ListNode *)s_ppogatt_head,
                                    prv_forward_connection_filter_callback,
                                    (void *)(uintptr_t)conn_handle);
}

// -------------------------------------------------------------------------------------------------

static bool prv_client_filter_callback(ListNode *found_node, void *data) {
  return ((const PPoGATTClient *)found_node == (const PPoGATTClient *)data);
}

static bool prv_is_client_valid(const PPoGATTClient *client) {
  return (list_find((ListNode *)s_ppogatt_head, prv_client_filter_callback, (void *)client) !=
          nullptr);
}

// -------------------------------------------------------------------------------------------------

static uint16_t prv_get_max_payload_size(const PPoGATTClient *client) {
  // 0 if the connection is gone.
  const uint16_t mtu = ble_att_mtu(prv_get_conn_handle(client));
  if (mtu < PPOGATT_MTU_MIN) {
    // Device got disconnected in the mean time
    return 0;
  }
  const uint16_t num_bytes_overhead = 3 /* ATT header */ + sizeof(PPoGATTPacket);
  return (mtu - num_bytes_overhead);
}

// -------------------------------------------------------------------------------------------------

static void prv_enter_awaiting_reset_complete(PPoGATTClient *client, bool self_initiated) {
  if (client->state == StateConnectedOpen) {
    // Closing the session drops what is left in its send queue.
    pbl_pp_session_close(client->session);
    client->session = nullptr;
  }
  client->in.next_expected_data_sn = 0;
  // TODO: use the SN for Reset Request / Reset Complete
  client->out = (typeof(client->out)){};

  if (prv_client_supports_enhanced_throughput_features(client)) {
    // Set our desired window sizes: the largest TX window, the phone caps it
    client->out.tx_window_size = (PPOGATT_SN_MOD_DIV - 1);

    const uint8_t desired_rx_window =
        PBL_MIN(PPOGATT_V1_DESIRED_RX_WINDOW_SIZE, PPOGATT_SN_MOD_DIV - 1);
    client->out.rx_window_size = desired_rx_window;
  } else {
    client->out.tx_window_size = client->out.rx_window_size = PPOGATT_V0_WINDOW_SIZE;
  }

  if (self_initiated) {
    client->out.reset_packet_to_send.type = PPoGATTPacketTypeResetRequest;
    client->state = StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset;
  } else {
    client->out.reset_packet_to_send.type = PPoGATTPacketTypeResetComplete;
    client->state = StateConnectedClosedAwaitingResetCompleteRemoteInitiatedReset;
  }
  prv_send_next_packets(client);

  // Set a timeout within which we expect to receive the "Reset Complete" message.
  prv_reset_ack_timeout(client);
}

// -------------------------------------------------------------------------------------------------

static void prv_start_reset(PPoGATTClient *client) {
  if (++client->resets_counter >= PPOGATT_RESET_COUNT_MAX) {
    if (client->disconnect_requested) {
      return;
    }

    if (++s_disconnect_counter > PPOGATT_DISCONNECT_COUNT_MAX) {
      // If we have disconnected too many times, do not disconnect and leave the client in a
      // "stalled" state, so that we have the option to "unstall" by sending a remote reset
      client->state = StateConnectedClosedAwaitingResetCompleteSelfInitiatedResetStalled;
      PBL_LOG_WRN("Reset request stalled, not disconnecting");
      return;
    }

    PBL_LOG_ERR("Disconnecting because max resets reached...");

    const int rc = ble_gap_terminate(prv_get_conn_handle(client), BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
      PBL_LOG_ERR("Could not disconnect (%d)", rc);
    }
    client->disconnect_requested = true;
  } else {
    prv_enter_awaiting_reset_complete(client, true /* self_initiated */);
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_reset_request(PPoGATTClient *client) {
  if (client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset) {
    // Already in self-initiated reset procedure, client should ignore the request from the server.
    PBL_LOG_INFO("Ignoring reset request because local client already requested.");
    return;
  }
  if (client->state == StateConnectedClosedAwaitingResetCompleteRemoteInitiatedReset) {
    // Already in remote-initiated reset procedure, server retrying?
    PBL_LOG_INFO("Ignoring reset request because remote server already requested.");
    return;
  }
  prv_enter_awaiting_reset_complete(client, false /* self_initiated */);
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_reset_complete(PPoGATTClient *client, const PPoGATTPacket *packet,
                                      uint16_t payload_length) {
  struct pbl_pp_session *session =
      pbl_pp_session_open(&s_transport, client->destination, &client->app_uuid);
  if (!session) {
    prv_delete_client(client, false /* is_disconnected */, DeleteReason_CouldntOpenSession);
    return;
  }

  // Possibly successful disconnect?
  if (s_disconnect_counter) {
  }
  pbl_pp_gatt_reset_disconnect_counter();
  client->resets_counter = 0;

  if (PBL_LIKELY(client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset)) {
    client->out.reset_packet_to_send = (const PPoGATTPacketHeader){
      .sn = 0,
      .type = PPoGATTPacketTypeResetComplete,
    };
    prv_send_next_packets(client);
  }
  client->state = StateConnectedOpen;
  client->session = session;

  PBL_ANALYTICS_SET_UNSIGNED(ppog_reversed, (client->role == PPoGATTRoleReversed) ? 1 : 0);

  if (prv_client_supports_enhanced_throughput_features(client)) {
    if (payload_length < sizeof(PPoGATTResetCompleteClientIDPayloadV1)) {
      PBL_LOG_WRN("Unexpected PPoGatt Reset Complete Payload Size: %" PRIu16, payload_length);
      // Be defensive, and use the original window size
      client->out.tx_window_size = client->out.rx_window_size = PPOGATT_V0_WINDOW_SIZE;
    } else {
      PPoGATTResetCompleteClientIDPayloadV1 *payload =
          (PPoGATTResetCompleteClientIDPayloadV1 *)&packet->payload[0];
      PBL_LOG_DBG("PPoGATT Remote RxWindow: %d TxWindow %d", payload->ppogatt_max_rx_window,
                  payload->ppogatt_max_tx_window);

      client->out.tx_window_size =
          PBL_MIN(client->out.tx_window_size, payload->ppogatt_max_rx_window);
      client->out.rx_window_size =
          PBL_MIN(client->out.rx_window_size, payload->ppogatt_max_tx_window);
    }
  }

  {
    const uint32_t elapsed_ms = (pbl_uptime_ticks() - client->created_ticks) * 1000 / PBL_TICK_HZ;
    PBL_LOG_INFO("PPoGATT Session is opened (%s, Vers: %d TXW: %d RXW: %d) after %" PRIu32 "ms!",
                 (client->role == PPoGATTRoleReversed) ? "reversed" : "forward", client->version,
                 client->out.tx_window_size, client->out.rx_window_size, elapsed_ms);
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_ack(PPoGATTClient *client, uint32_t sn) {
  if (prv_is_packet_with_sn_awaiting_ack(client, sn)) {
    client->out.timeouts_counter = 0;
    client->out.ack_timeout_state = AckTimeoutState_Inactive;
    prv_update_ack_timer();

    // Ack'd one of the packets in flight
    const uint32_t next_sn = prv_next_sn(sn);
    const uint16_t num_bytes_acked = prv_total_num_bytes_awaiting_ack_up_to(client, next_sn);
    pbl_pp_session_send_queue_consume(client->session, num_bytes_acked);

    // If next_data_sn is before the Ack'd sn, the packet pending retransmission has just been
    // Ack'd. We can determine whether or not a packet is pending retransmission by checking if the
    // payload size for next_data_sn is not 0. This means the packet has been enqueued to get sent
    if (prv_is_packet_with_sn_awaiting_ack(client, client->out.next_data_sn)) {
      client->out.next_data_sn = next_sn;
    }

    // Clear up the payload size(s) for Ack'd packets.
    // See comment with the prv_get_payload_size_for_sn() call in prv_send_next_packets()
    prv_clear_payload_sizes_up_to(client, next_sn);

    client->out.next_expected_ack_sn = next_sn;

    if (prv_get_payload_size_for_sn(client, next_sn) != 0) { // Still awaiting ACKs
      prv_reset_ack_timeout(client);
    }

    prv_send_next_packets(client);
  } else if (sn == prv_prev_sn(client->out.next_expected_ack_sn)) {
    // Data we had sent got dropped causing the other side to re-ACK the last data it had received.
    // Don't roll back directly to avoid creating an Sorcerer's Apprentice bug
    // https://en.wikipedia.org/wiki/Sorcerer%27s_Apprentice_Syndrome
    // We'll rely on the ACK timeout for the next data packet to fire and roll back.
    PBL_LOG_WRN("Received retransmitted Ack for sn:%" PRIu32 ". Ignoring it.", sn);
  } else {
    PBL_LOG_ERR("Ack'd packet out of range %" PRIu32 ", [%u-%u].", sn,
                client->out.next_expected_ack_sn, client->out.next_data_sn);
    prv_start_reset(client);
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_data(PPoGATTClient *client, const PPoGATTPacket *packet,
                            uint16_t payload_length) {
  if (client->in.next_expected_data_sn == packet->sn) {
    client->out.ack_packet_to_send = (const PPoGATTPacketHeader){
      .sn = client->in.next_expected_data_sn,
      .type = PPoGATTPacketTypeAck,
    };
    if (!prv_client_supports_enhanced_throughput_features(client)) {
      client->out.send_rx_ack_now = true;
    } else {
      client->out.outstanding_rx_ack_count++;
      if (client->out.outstanding_rx_ack_count >= (client->out.rx_window_size / 2)) {
        // we want to ACK data before the other side is blocking waiting for an ACK
        client->out.send_rx_ack_now = true;
      }
    }
    prv_send_next_packets(client);

    client->in.next_expected_data_sn = prv_next_sn(client->in.next_expected_data_sn);
    pbl_pp_session_receive(client->session, packet->payload, payload_length);
    //    PBL_LOG_DBG("Got PP data (sn=%u, %u bytes)", packet->sn, payload_length);
  } else {
    PBL_LOG_DBG("packet->sn != next_expected_data_sn (%u != %u)", packet->sn,
                client->in.next_expected_data_sn);
    // Rely on the server retransmitting on Ack time-out
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_data_notification(PPoGATTClient *client, const uint8_t *value,
                                         uint16_t value_length) {
  //  PBL_LOG_DBG("IN:");
  //  PBL_LOG_HEXDUMP_DBG(value, value_length);

  if (PBL_UNLIKELY(value_length == 0)) {
    PBL_LOG_ERR("Zero length packet");
    return;
  }
  const PPoGATTPacket *packet = (const PPoGATTPacket *)value;
  if (PBL_UNLIKELY(packet->type >= PPoGATTPacketTypeInvalidRangeStart)) {
    PBL_LOG_ERR("Invalid type %u", packet->type);
    return;
  }
  if (PBL_UNLIKELY(packet->type == PPoGATTPacketTypeResetRequest)) {
    PBL_LOG_INFO("Got reset request!");
    if (client->role == PPoGATTRoleReversed &&
        s_reversed_active_conn_handle != client->rev.conn_handle) {
      prv_reversed_take_over(client);
    }
    prv_handle_reset_request(client);
    return;
  }
  if (PBL_LIKELY(client->state == StateConnectedOpen)) {
    if (PBL_LIKELY(packet->type == PPoGATTPacketTypeData)) {
      prv_handle_data(client, packet, value_length - sizeof(PPoGATTPacket));
    } else if (PBL_LIKELY(packet->type == PPoGATTPacketTypeAck)) {
      prv_handle_ack(client, packet->sn);
    } else if (PBL_UNLIKELY(packet->type == PPoGATTPacketTypeResetComplete)) {
      PBL_LOG_ERR("Got reset complete while open!?");
    }
  } else if (client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedReset ||
             client->state == StateConnectedClosedAwaitingResetCompleteSelfInitiatedResetStalled ||
             client->state == StateConnectedClosedAwaitingResetCompleteRemoteInitiatedReset) {
    if (PBL_LIKELY(packet->type == PPoGATTPacketTypeResetComplete)) {
      prv_handle_reset_complete(client, packet, value_length - sizeof(PPoGATTPacket));
    } else {
      PBL_LOG_DBG("Resetting, ignoring data/ack packets (%u)", packet->type);
    }
  } else {
    PBL_LOG_DBG("Ignoring all packets in state %u", client->state);
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_rediscover_main_cb(void *data) {
  // The handles may be stale: forget the services, and look for them again.
  prv_forward_restart();
}

static void prv_rediscover_work_cb(void *data) {
  pbl_pp_main_call(prv_rediscover_main_cb, nullptr);
}

static void prv_request_meta_rediscovery(PPoGATTClient *client) {
  pbl_mutex_assert_held(&s_lock, true);

  // Not from here: the caller is about to delete the client, which looking again does as well.
  pbl_sys_work_call(prv_rediscover_work_cb, nullptr);
}

static void prv_retry_meta_read(PPoGATTClient *client) {
  pbl_mutex_assert_held(&s_lock, true);

  if (!client || client->state != StateDisconnectedAwaitingMetaRetry) {
    return;
  }

  PBL_LOG_INFO("Retrying PPoGATT meta read (attempt %u/%u)", client->meta_read_retries + 1,
               PPOGATT_META_READ_RETRY_COUNT_MAX);

  client->state = StateDisconnectedReadingMeta;
  const uint8_t instance = client->fwd.instance;

  // Release the lock before calling into NimBLE.
  prv_unlock();

  const int result =
      pbl_bt_gattc_service_read(&s_forward_services[instance], PBL_PP_GATT_CHARACTERISTIC_META);

  // Re-acquire the lock before accessing client state again
  prv_lock();

  client = prv_find_forward_client_with_instance(instance);
  if (!client) {
    return;
  }

  if (result != 0) {
    // Read failed to start, delete the client
    PBL_LOG_ERR("Failed to initiate meta read retry");
    prv_delete_client(client, false /* is_disconnected */, DeleteReason_MetaDataReadFailure);
  }
}

static void prv_meta_read_retry_main_cb(void *data) {
  PPoGATTClient *client = (PPoGATTClient *)data;
  prv_lock();
  {
    // make sure we didn't disconnect in between
    if (prv_is_client_valid(client)) {
      prv_retry_meta_read(client);
    }
  }
  prv_unlock();
}

//! Never block on the lock from the work queue; bounce to the main thread
//! (prv_retry_meta_read also calls into NimBLE, which doesn't belong on the work queue).
static void prv_meta_read_retry_fn(struct pbl_work *work) {
  pbl_pp_main_call(prv_meta_read_retry_main_cb,
                   PBL_CONTAINER_OF(work, PPoGATTClient, meta_read_retry_work.work));
}

// -------------------------------------------------------------------------------------------------

static void prv_handle_meta_read(PPoGATTClient *client, const uint8_t *value, size_t value_length,
                                 int error) {
  const uint32_t elapsed_ms = (pbl_uptime_ticks() - client->created_ticks) * 1000 / PBL_TICK_HZ;
  PBL_ASSERTN(client->state == StateDisconnectedReadingMeta);
  if (error != 0) {
    PBL_LOG_ERR("PPoGATT meta read failed: err=%d after %" PRIu32 "ms (retry %u/%u)", error,
                elapsed_ms, client->meta_read_retries, PPOGATT_META_READ_RETRY_COUNT_MAX);
    // GATT read failed - this is retriable since the mobile app may not be ready yet
    goto handle_retriable_error;
  }
  PBL_LOG_INFO("PPoGATT meta read success after %" PRIu32 "ms, len=%u", elapsed_ms,
               (unsigned int)value_length);
  if (value_length < sizeof(PPoGATTMetaV0)) {
    goto handle_error;
  }
  const PPoGATTMetaV0 *meta = (const PPoGATTMetaV0 *)value;
  if (meta->ppogatt_min_version > PPOGATT_MAX_VERSION
      /* || meta->ppogatt_max_version < PPOGATT_MIN_VERSION  // always true at the moment */) {
    goto handle_error;
  }
  if (uuid_is_invalid(&meta->app_uuid)) {
    PBL_LOG_ERR("Invalid UUID");
    goto handle_error;
  }
#ifdef CONFIG_PP_TRANSPORT_GATT_SYSTEM_ONLY
  if (!uuid_is_system(&meta->app_uuid)) {
    PBL_LOG_ERR("Found PPoGATT server from non-Pebble app, not connecting in PRF..");
    goto handle_error;
  }
#endif

  // Use the highest version that both ends support:
  client->version = PBL_MIN(meta->ppogatt_max_version, PPOGATT_MAX_VERSION);

  // Parse additional v1 metadata fields:
  PPoGATTSessionType session_type = PPoGATTSessionType_InferredFromUuid;
  if (/*meta->ppogatt_max_version >= 0x01 &&*/ value_length >= sizeof(PPoGATTMetaV1)) {
    const PPoGATTMetaV1 *meta_v1 = (const PPoGATTMetaV1 *)value;
    if (meta_v1->pp_session_type >= PPoGATTSessionTypeCount) {
      PBL_LOG_ERR("Invalid session type %u", meta_v1->pp_session_type);
      goto handle_error;
    }
    session_type = meta_v1->pp_session_type;
  }

  // Save data needed across the the lock release
  const uint8_t instance = client->fwd.instance;
  Uuid app_uuid = meta->app_uuid;

  // Release the lock before subscribing, which calls into NimBLE.
  prv_unlock();

  const int e = pbl_bt_gattc_service_subscribe(&s_forward_services[instance],
                                               PBL_PP_GATT_CHARACTERISTIC_DATA, true);

  // Re-acquire the lock before accessing client state
  prv_lock();

  // Re-validate client pointer after releasing lock, as it may have been removed
  client = prv_find_forward_client_with_instance(instance);
  if (!client) {
    return;
  }

  if (e == 0) {
    // Delete any existing client with this UUID, last one wins.
    // iOS behavior is a bit strange when it comes to service persistence. When an app crashes or
    // gets killed through Xcode, the service records persist. When the app is relaunched again,
    // a new service will get added again. The old one remains when it was killed through Xcode
    // before. The old one seems to go away *after* the new one gets added in the crash scenario.
    PPoGATTClient *existing_client = prv_find_client_with_uuid(&app_uuid);
    if (existing_client) {
      PBL_LOG_ERR("Found PPoGATT server with same UUID. Keeping only the last one.");
      prv_delete_client(existing_client, true /* is_disconnected */, DeleteReason_DuplicateServer);
    }
    client->state = StateDisconnectedSubscribingData;
    client->app_uuid = app_uuid;
    // Reset retry counter on success
    client->meta_read_retries = 0;

    if (session_type == PPoGATTSessionType_Hybrid) {
      client->destination = PBL_PP_DESTINATION_HYBRID;
    } else { // (session_type == PPoGATTSessionType_InferredFromUuid)
      const bool is_system = uuid_is_system(&client->app_uuid);
      client->destination = is_system ? PBL_PP_DESTINATION_SYSTEM : PBL_PP_DESTINATION_APP;
    }
    return;
  }

handle_retriable_error:
  // GATT read failed - schedule a retry if we haven't exceeded the max retry count
  if (++client->meta_read_retries < PPOGATT_META_READ_RETRY_COUNT_MAX) {
    PBL_LOG_WRN("PPoGATT meta read failed (err=%d), scheduling retry %u/%u", error,
                client->meta_read_retries, PPOGATT_META_READ_RETRY_COUNT_MAX);
    client->state = StateDisconnectedAwaitingMetaRetry;
    pbl_delayed_work_reschedule(&pbl_sys_workq_high, &client->meta_read_retry_work,
                                PBL_MSEC(PPOGATT_META_READ_RETRY_DELAY_MS));
    return;
  }
  PBL_LOG_ERR("PPoGATT meta read failed after %u retries", PPOGATT_META_READ_RETRY_COUNT_MAX);

  // Cached handle may be stale; try once with fresh discovery.
  if (!s_rediscovery_requested_this_connection) {
    PBL_LOG_INFO("Triggering GATT rediscovery for fresh PPoGATT handles");
    s_rediscovery_requested_this_connection = true;
    prv_request_meta_rediscovery(client);
  }

handle_error:
  PBL_LOG_ERR("Failed handling PPoGATT meta: len=%u ver=%x err=%d", (unsigned int)value_length,
              value_length ? value[0] : ~0, error);
  prv_delete_client(client, false /* is_disconnected */, DeleteReason_MetaDataInvalid);
}

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_create(void) {
  prv_lock();
  {
    PBL_ASSERTN(pbl_pp_main_is_current());
    PBL_ASSERTN(!s_ppogatt_head);
    s_rediscovery_requested_this_connection = false;
    prv_forward_init();
  }
  prv_unlock();
}

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_invalidate_all_references(void) {
  prv_lock();
  {
    PPoGATTClient *client = s_ppogatt_head;
    while (client) {
      PPoGATTClient *next = (PPoGATTClient *)client->node.next;
      // Reversed clients live on our own GATT server, nothing would recreate them
      if (client->role != PPoGATTRoleReversed) {
        prv_delete_client(client, true /* is_disconnected */, DeleteReason_InvalidateAllReferences);
      }
      client = next;
    }
  }
  prv_unlock();
}

// -------------------------------------------------------------------------------------------------
// Reversed PPoG (watch hosts the GATT service)

// A subscription alone doesn't mean the phone uses reversed PPoG: NimBLE also
// restores a bonded phone's subscription when it reconnects, while the phone
// may host forward PPoG. Reversed takes over once the phone starts its
// handshake.
static void prv_reversed_take_over(PPoGATTClient *client) {
  pbl_mutex_assert_held(&s_lock, true);

  PPoGATTClient *forward = prv_find_forward_client_for_conn(client->rev.conn_handle);
  if (forward) {
    PBL_LOG_INFO("Reversed PPoG taking over from forward client");
    prv_delete_client(forward, false /* is_disconnected */, DeleteReason_DuplicateServer);
  }
  s_reversed_active_conn_handle = client->rev.conn_handle;
}

static void prv_reversed_start(uint16_t conn_handle) {
  pbl_mutex_assert_held(&s_lock, true);

  PPoGATTClient *client = prv_create_client();
  if (!client) {
    return;
  }
  client->role = PPoGATTRoleReversed;
  client->rev.conn_handle = conn_handle;
  client->app_uuid = UUID_INVALID;
  client->version = PPOGATT_MAX_VERSION;
  client->destination = PBL_PP_DESTINATION_HYBRID;

  // The handshake is phone-initiated: wait for the phone's ResetRequest.
  client->state = StateConnectedClosedAwaitingResetRequest;
}

void ppogatt_reversed_handle_subscribed(uint16_t conn_handle) {
  struct ble_gap_conn_desc desc;

  if (ble_gap_conn_find(conn_handle, &desc) != 0) {
    PBL_LOG_WRN("Reversed PPoG subscribed: no connection");
    return;
  }
  // Backstop for the check of the service: never talk PP on an unencrypted link.
  if (!desc.sec_state.encrypted) {
    PBL_LOG_WRN("Reversed PPoG subscribed: connection not encrypted; ignoring");
    return;
  }

  prv_lock();
  {
    PBL_LOG_INFO("Reversed PPoG subscribed (conn_handle=%u)", conn_handle);
    prv_reversed_start(conn_handle);
  }
  prv_unlock();
}

void ppogatt_reversed_handle_unsubscribed(uint16_t conn_handle) {
  prv_lock();
  {
    PPoGATTClient *client = prv_find_reversed_client_for_conn(conn_handle);
    if (client) {
      PBL_LOG_INFO("Reversed PPoG unsubscribed (conn_handle=%u)", conn_handle);
      prv_delete_client(client, false /* is_disconnected */, DeleteReason_CloseCalled);
    }
  }
  prv_unlock();
}

void ppogatt_reversed_handle_data(uint16_t conn_handle, const uint8_t *buf, size_t len) {
  prv_lock();
  {
    PPoGATTClient *client = prv_find_reversed_client_for_conn(conn_handle);
    if (!client) {
      PBL_LOG_DBG("Reversed PPoG data for unknown conn_handle=%u", conn_handle);
      goto unlock;
    }
    prv_handle_data_notification(client, buf, (uint16_t)len);
  }
unlock:
  prv_unlock();
}

// The pbl_bt_cb_* callbacks below defer to the main thread: the NimBLE host-task
// stack is too small to run the PPoG state machine directly, and SysWorkq
// callbacks may block waiting for Pebble Protocol send progress (e.g. the BT
// log dump), which would starve inbound ACK processing and deadlock the
// transport. The main thread is where forward-mode RX and send_next jobs already
// run.

typedef struct {
  uint16_t conn_handle;
  uint16_t len;
  uint8_t *buf; // owned, freed by the main thread cb.
} ReversedDataCtx;

static void prv_reversed_subscribed_main_cb(void *data) {
  ppogatt_reversed_handle_subscribed((uint16_t)(uintptr_t)data);
}

static void prv_reversed_unsubscribed_main_cb(void *data) {
  ppogatt_reversed_handle_unsubscribed((uint16_t)(uintptr_t)data);
}

static void prv_reversed_data_main_cb(void *data) {
  ReversedDataCtx *ctx = (ReversedDataCtx *)data;
  ppogatt_reversed_handle_data(ctx->conn_handle, ctx->buf, ctx->len);
  pbl_free(ctx->buf);
  pbl_free(ctx);
}

void pbl_bt_cb_ppog_reversed_subscribed(uint16_t conn_handle) {
  pbl_pp_main_call(prv_reversed_subscribed_main_cb, (void *)(uintptr_t)conn_handle);
}

void pbl_bt_cb_ppog_reversed_unsubscribed(uint16_t conn_handle) {
  pbl_pp_main_call(prv_reversed_unsubscribed_main_cb, (void *)(uintptr_t)conn_handle);
}

void pbl_bt_cb_ppog_reversed_data_written(uint16_t conn_handle, uint8_t *buf, uint16_t len) {
  ReversedDataCtx *ctx = pbl_malloc(sizeof(*ctx));
  if (!ctx) {
    PBL_LOG_ERR("Reversed PPoG data: out of memory");
    pbl_free(buf);
    return;
  }
  ctx->conn_handle = conn_handle;
  ctx->len = len;
  ctx->buf = buf;
  pbl_pp_main_call(prv_reversed_data_main_cb, ctx);
}

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_handle_service_discovered(uint8_t instance) {
  PBL_LOG_INFO("PPoGATT service discovered, starting handshake");

  if (ble_gap_conn_find(s_forward_conn_handle, nullptr) != 0) {
    return;
  }

  prv_lock();
  {
    // Reversed PPoG has priority: ignore the forward service if reversed is
    // active on this connection.
    if (s_reversed_active_conn_handle == s_forward_conn_handle) {
      prv_unlock();
      PBL_LOG_DBG("Reversed PPoG active, skipping forward");
      return;
    }

    // Create new clients:
    PPoGATTClient *client = prv_create_client();
    if (!client) {
      prv_unlock();
      return;
    }
    client->role = PPoGATTRoleForward;
    client->fwd.instance = instance;

    PBL_LOG_DBG("PPoGATT reading meta characteristic");

    // Release the lock before calling into NimBLE.
    prv_unlock();

    const int result =
        pbl_bt_gattc_service_read(&s_forward_services[instance], PBL_PP_GATT_CHARACTERISTIC_META);

    // Re-acquire the lock before accessing client state again
    prv_lock();

    client = prv_find_forward_client_with_instance(instance);
    if (client && result != 0) {
      PBL_LOG_ERR("PPoGATT meta read initiation failed: err=%d", result);
      // Read failed, probably disconnected or insufficient resources
      prv_delete_client(client, false /* is_disconnected */, DeleteReason_MetaDataReadFailure);
    }
  }
  prv_unlock();
}

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_handle_subscribe(uint8_t instance, bool is_subscribed, int error) {
  prv_lock();
  {
    PPoGATTClient *client = prv_find_forward_client_with_instance(instance);
    if (!client && is_subscribed) {
      PBL_LOG_ERR("PPoGATT Client could be found, unsubscribing");
      // Attempt to unsubscribe to avoid wasting bandwidth.
      // Release the lock before calling into NimBLE.
      prv_unlock();
      (void)pbl_bt_gattc_service_subscribe(&s_forward_services[instance],
                                           PBL_PP_GATT_CHARACTERISTIC_DATA, false);
      prv_lock();
      goto unlock;
    }
    if (!is_subscribed) {
      // Unsubscribed due to removed client
      goto unlock;
    }
    PBL_ASSERTN(client->state == StateDisconnectedSubscribingData);
    if (error) {
      const uint32_t elapsed_ms = (pbl_uptime_ticks() - client->created_ticks) * 1000 / PBL_TICK_HZ;
      PBL_LOG_ERR("PPoGATT subscribe failed: err=%d after %" PRIu32 "ms", error, elapsed_ms);
      prv_delete_client(client, false /* is_disconnected */, DeleteReason_SubscribeFailure);
      goto unlock;
    }
    {
      const uint32_t elapsed_ms = (pbl_uptime_ticks() - client->created_ticks) * 1000 / PBL_TICK_HZ;
      PBL_LOG_INFO("PPoGATT subscribed to Data after %" PRIu32 "ms, sending ResetRequest",
                   elapsed_ms);
    }
    prv_start_reset(client);
  }
unlock:
  prv_unlock();
}

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_handle_read_or_notification(uint8_t instance,
                                             enum pbl_pp_gatt_characteristic characteristic,
                                             const uint8_t *value, size_t value_length, int error) {
  prv_lock();
  {
    PPoGATTClient *client = prv_find_forward_client_with_instance(instance);
    if (!client) {
      PBL_LOG_DBG("Got notification/read for unknown client");
      goto unlock;
    }
    if (characteristic == PBL_PP_GATT_CHARACTERISTIC_DATA) {
      prv_handle_data_notification(client, value, value_length);
    } else if (client->state == StateDisconnectedReadingMeta) {
      prv_handle_meta_read(client, value, value_length, error);
    }
  }
unlock:
  prv_unlock();
}

// -------------------------------------------------------------------------------------------------
// The service clients call back on the host task; the state machine runs on the main thread.

typedef enum {
  ForwardEventDiscovered,
  ForwardEventSubscribed,
  ForwardEventUnsubscribed,
  ForwardEventRead,
  ForwardEventNotified,
} ForwardEventType;

typedef struct {
  ForwardEventType type;
  uint32_t generation;
  uint8_t instance;
  uint8_t characteristic;
  int status;
  uint16_t length;
  uint8_t data[];
} ForwardEvent;

static void prv_forward_invalidate(uint8_t instance) {
  prv_lock();
  {
    PPoGATTClient *client = prv_find_forward_client_with_instance(instance);
    if (client) {
      prv_delete_client(client, true /* is_disconnected */, DeleteReason_InvalidateAllReferences);
    }
  }
  prv_unlock();
}

static void prv_forward_event_main_cb(void *data) {
  ForwardEvent *event = data;

  if (event->generation != s_forward_generation) {
    pbl_free(event);
    return;
  }

  switch (event->type) {
    case ForwardEventDiscovered:
      // Found again elsewhere, or gone: what there was is of no use.
      prv_forward_invalidate(event->instance);
      if (event->status == 0) {
        pbl_pp_gatt_handle_service_discovered(event->instance);
        // An app of the phone may host one more.
        if (event->instance + 1 < FORWARD_SERVICES_MAX) {
          prv_forward_discover(event->instance + 1);
        }
      } else {
        pbl_bt_gattc_service_stop(&s_forward_services[event->instance]);
      }
      break;
    case ForwardEventSubscribed:
    case ForwardEventUnsubscribed:
      pbl_pp_gatt_handle_subscribe(event->instance, event->type == ForwardEventSubscribed,
                                   event->status);
      break;
    case ForwardEventRead:
    case ForwardEventNotified:
      pbl_pp_gatt_handle_read_or_notification(event->instance, event->characteristic, event->data,
                                              event->length, event->status);
      break;
  }

  pbl_free(event);
}

static void prv_put_forward_event(struct pbl_bt_gattc_service *svc, ForwardEventType type,
                                  uint8_t characteristic, int status, const uint8_t *data,
                                  uint16_t length) {
  ForwardEvent *event = pbl_malloc(sizeof(*event) + length);
  if (!event) {
    PBL_LOG_ERR("PPoGATT event %d: out of memory", type);
    return;
  }

  *event = (ForwardEvent){
    .type = type,
    .generation = s_forward_generation,
    .instance = svc - s_forward_services,
    .characteristic = characteristic,
    .status = status,
    .length = length,
  };
  if (length > 0) {
    memcpy(event->data, data, length);
  }
  pbl_pp_main_call(prv_forward_event_main_cb, event);
}

static void prv_forward_discovered(struct pbl_bt_gattc_service *svc, int status) {
  prv_put_forward_event(svc, ForwardEventDiscovered, 0, status, nullptr, 0);
}

static void prv_forward_subscribed(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable,
                                   int status) {
  prv_put_forward_event(svc, enable ? ForwardEventSubscribed : ForwardEventUnsubscribed, chr,
                        status, nullptr, 0);
}

static void prv_forward_notified(struct pbl_bt_gattc_service *svc, uint8_t chr, const uint8_t *data,
                                 uint16_t len) {
  prv_put_forward_event(svc, ForwardEventNotified, chr, 0, data, len);
}

static void prv_forward_read(struct pbl_bt_gattc_service *svc, uint8_t chr, int status,
                             const uint8_t *data, uint16_t len) {
  prv_put_forward_event(svc, ForwardEventRead, chr, status, data, len);
}

static const struct pbl_bt_gattc_service_cb s_forward_cb = {
  .discovered = prv_forward_discovered,
  .subscribed = prv_forward_subscribed,
  .notified = prv_forward_notified,
  .read = prv_forward_read,
};

static void prv_forward_init(void) {
  for (int i = 0; i < FORWARD_SERVICES_MAX; i++) {
    struct pbl_bt_gattc_service *svc = &s_forward_services[i];

    if (svc->chrs != nullptr) {
      continue;
    }
    *svc = (struct pbl_bt_gattc_service){
      .svc_uuid = &s_forward_svc_uuid.u,
      .chr_uuids = s_forward_chr_uuids,
      .notifying_chrs = 1U << PBL_PP_GATT_CHARACTERISTIC_DATA,
      .num_chrs = PBL_PP_GATT_CHARACTERISTIC_NUM,
      .instance = i,
      .chrs = s_forward_chrs[i],
    };
  }
}

static void prv_forward_discover(uint8_t instance) {
  struct pbl_bt_gattc_service *svc = &s_forward_services[instance];

  const int rc = pbl_bt_gattc_service_start(svc, s_forward_conn_handle, &s_forward_cb);
  if (rc != 0 && rc != -EALREADY) {
    PBL_LOG_ERR("Could not look for PPoGATT service %u (%d)", instance, rc);
  }
}

static void prv_forward_stop(void) {
  ++s_forward_generation;
  prv_forward_init();
  for (int i = 0; i < FORWARD_SERVICES_MAX; i++) {
    pbl_bt_gattc_service_stop(&s_forward_services[i]);
  }
}

void pbl_pp_gatt_discover(uint16_t conn_handle) {
  PBL_ASSERTN(pbl_pp_main_is_current());
  prv_forward_stop();
  s_forward_conn_handle = conn_handle;
  prv_forward_discover(0);
}

static void prv_forward_restart(void) {
  pbl_pp_gatt_invalidate_all_references();
  prv_forward_stop();
  prv_forward_discover(0);
}

void pbl_pp_gatt_handle_services_changed(void) {
  PBL_ASSERTN(pbl_pp_main_is_current());
  // A service that is where it was keeps its session.
  for (int i = 0; i < FORWARD_SERVICES_MAX; i++) {
    if (pbl_bt_gattc_service_rediscover(&s_forward_services[i]) == -ENOTCONN) {
      // The first that was not there: it may be now.
      prv_forward_discover(i);
      break;
    }
  }
}

static bool prv_forward_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) {
  for (int i = 0; i < FORWARD_SERVICES_MAX; i++) {
    if (pbl_bt_gattc_service_notify_rx(&s_forward_services[i], conn_handle, attr_handle, om)) {
      return true;
    }
  }
  return false;
}

PBL_BT_GATTC_NOTIFY_LISTENER_DEFINE(ppogatt, prv_forward_notify_rx);

// -------------------------------------------------------------------------------------------------

static void prv_retry_send_later(PPoGATTClient *client);

static PPoGATTPacket *prv_lazily_allocate_packet_if_needed(PPoGATTClient *client,
                                                           PPoGATTPacket **heap_packet_in_out) {
  PPoGATTPacket *packet = *heap_packet_in_out;
  // Lazily allocate a buffer on the heap:
  if (!packet) {
    const uint16_t max_payload_size = prv_get_max_payload_size(client);
    if (max_payload_size == 0) {
      return nullptr;
    }
    packet = (PPoGATTPacket *)pbl_malloc(sizeof(PPoGATTPacket) + max_payload_size);
    if (!packet) {
      prv_retry_send_later(client);
      return nullptr;
    }
    *heap_packet_in_out = packet;
  }
  return packet;
}

// -------------------------------------------------------------------------------------------------

static const PPoGATTPacket *prv_prepare_next_reset_packet(PPoGATTClient *client,
                                                          PPoGATTPacket **heap_packet_in_out,
                                                          uint16_t *payload_size_out) {
  PPoGATTPacket *packet = prv_lazily_allocate_packet_if_needed(client, heap_packet_in_out);
  if (!packet) {
    PBL_LOG_WRN("Couldn't allocate reset packet: role=%u state=%u type=%u", client->role,
                client->state, (unsigned)client->out.reset_packet_to_send.type);
    return nullptr;
  }

  if (client->out.reset_packet_to_send.type == PPoGATTPacketTypeResetRequest) {
    // Reset Request packet:
    *packet = (const PPoGATTPacket){
      .type = PPoGATTPacketTypeResetRequest,
      .sn = 0,
    };
    PPoGATTResetRequestClientIDPayload *client_id_payload =
        (PPoGATTResetRequestClientIDPayload *)packet->payload;
    *client_id_payload = (const PPoGATTResetRequestClientIDPayload){
      .ppogatt_version = client->version,
    };
    memcpy(client_id_payload->serial_number, pbl_pp_gatt_cb_get_serial_number(),
           sizeof(client_id_payload->serial_number));
    *payload_size_out = sizeof(*client_id_payload);
    return packet;
  } else { // PPoGATTPacketTypeResetComplete
    if (prv_client_supports_enhanced_throughput_features(client)) {
      *packet = (const PPoGATTPacket){
        .sn = 0,
        .type = PPoGATTPacketTypeResetComplete,
      };
      PPoGATTResetCompleteClientIDPayloadV1 *client_id_payload =
          (PPoGATTResetCompleteClientIDPayloadV1 *)packet->payload;
      *client_id_payload = (const PPoGATTResetCompleteClientIDPayloadV1){
        .ppogatt_max_rx_window = client->out.rx_window_size,
        .ppogatt_max_tx_window = client->out.tx_window_size,
      };
      *payload_size_out = sizeof(*client_id_payload);
      return packet;
    } else {
      // Reset Complete packet (zero payload size):
      *payload_size_out = 0;
      return (const PPoGATTPacket *)&client->out.reset_packet_to_send;
    }
  }
}

// -------------------------------------------------------------------------------------------------

static void prv_rx_ack_main_cb(void *data) {
  PPoGATTClient *client = (PPoGATTClient *)data;
  prv_lock();
  {
    // make sure we didn't disconnect in between
    if (prv_is_client_valid(client)) {
      client->out.send_rx_ack_now = true;
      prv_send_next_packets_async(client);
    }
  }
  prv_unlock();
}

//! Never block on the lock from the work queue; bounce to the main thread.
static void prv_rx_ack_fn(struct pbl_work *work) {
  pbl_pp_main_call(prv_rx_ack_main_cb, PBL_CONTAINER_OF(work, PPoGATTClient, rx_ack_work.work));
}

//! Pumps prv_send_next_packets directly (not via prv_send_next_packets_async)
//! because the session may not exist yet: a Reset Complete send can hit
//! ENOMEM during the handshake.
static void prv_send_retry_main_cb(void *data) {
  PPoGATTClient *client = (PPoGATTClient *)data;
  prv_lock();
  {
    // make sure we didn't disconnect in between
    if (prv_is_client_valid(client)) {
      prv_send_next_packets(client);
    }
  }
  prv_unlock();
}

//! Never send from the work queue (NimBLE calls don't belong on it); bounce
//! to the main thread, where the rest of the PPoG state machine runs.
static void prv_send_retry_fn(struct pbl_work *work) {
  pbl_pp_main_call(prv_send_retry_main_cb,
                   PBL_CONTAINER_OF(work, PPoGATTClient, send_retry_work.work));
}

//! Out of mbufs (e.g. inbound flood during a firmware update). NimBLE has no
//! "buffers freed" event, so retry later, backing off while the stall lasts.
static void prv_retry_send_later(PPoGATTClient *client) {
  if (pbl_delayed_work_is_pending(&client->send_retry_work)) {
    return;
  }
  if (client->send_retry_count == 0) {
    PBL_LOG_DBG("PPoGATT out of buffers, retrying");
  }
  // Double the delay per retry; the inner MIN only bounds the shift.
  const uint32_t delay_ms =
      PBL_MIN(PPOGATT_SEND_RETRY_DELAY_MS << PBL_MIN(client->send_retry_count, 16),
              PPOGATT_SEND_RETRY_MAX_DELAY_MS);
  client->send_retry_count++;
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &client->send_retry_work, PBL_MSEC(delay_ms));
}

static const PPoGATTPacket *prv_prepare_next_packet(PPoGATTClient *client,
                                                    PPoGATTPacket **heap_packet_in_out,
                                                    uint16_t *payload_size_out) {
  if (client->out.reset_packet_byte != 0) {
    return prv_prepare_next_reset_packet(client, heap_packet_in_out, payload_size_out);
  } else if (client->out.ack_packet_byte != 0) {
    if (client->out.send_rx_ack_now) {
      pbl_delayed_work_cancel(&client->rx_ack_work);

      // Ack packet (zero payload size):
      *payload_size_out = 0;
      return (const PPoGATTPacket *)&client->out.ack_packet_to_send;
    }

    pbl_delayed_work_schedule(&pbl_sys_workq_high, &client->rx_ack_work,
                              PBL_MSEC(PPOGATT_MAX_DATA_ACK_LATENCY_MS));
    // We will defer sending the Ack for now, fallthrough and send data instead
  }

  // Data packets:
  if (client->state != StateConnectedOpen) {
    return nullptr;
  };
  if (prv_num_packets_in_flight(client) >= client->out.tx_window_size) {
    // Max number of data packets in flight, try again when we got some of them Ack'd.
    return nullptr;
  }
  uint16_t read_space = pbl_pp_session_send_queue_length(client->session);
  if (read_space == 0) {
    return nullptr;
  }

  const uint16_t max_payload_size = prv_get_max_payload_size(client);
  if (!max_payload_size) {
    return nullptr;
  }

  // Bytes that are awaiting an Ack, have already been handed to the stack, but are still
  // sitting in the send buffer, until they are Ack'd in case we need to retransmit them.
  uint16_t offset = prv_total_num_bytes_awaiting_ack(client);

  // If retransmitting, we need to use the same fragmentation as the previous transmission.
  // The payload_sizes field will still contain the previously used size, unless it was zero'ed
  // out because it got Ack'd.
  uint16_t payload_size = prv_get_payload_size_for_sn(client, client->out.next_data_sn);
  if (payload_size == 0) {
    PBL_ASSERTN(read_space >= offset);
    payload_size = read_space - offset;

    if (payload_size == 0) {
      // No data to send
      return nullptr;
    }

    // Cap to the size that the GATT MTU allows:
    payload_size = PBL_MIN(payload_size, max_payload_size);
  }

  PPoGATTPacket *packet = prv_lazily_allocate_packet_if_needed(client, heap_packet_in_out);
  if (!packet) {
    return nullptr;
  }
  packet->type = PPoGATTPacketTypeData;
  packet->sn = client->out.next_data_sn;
  PBL_ASSERTN(
      pbl_pp_session_send_queue_copy(client->session, offset, payload_size, packet->payload));
  *payload_size_out = payload_size;
  return packet;
}

// -------------------------------------------------------------------------------------------------

static void prv_finalize_queued_packet(PPoGATTClient *client, uint16_t payload_size) {
  if (client->out.reset_packet_byte != 0) {
    client->out.reset_packet_byte = 0;
  } else if (client->out.send_rx_ack_now && client->out.ack_packet_byte != 0) {
    client->out.ack_packet_byte = 0;
    client->out.send_rx_ack_now = false;
    client->out.outstanding_rx_ack_count = 0;
  } else { // we are sending a data packet
    const uint32_t sn = client->out.next_data_sn;
    prv_set_payload_size_for_sn(client, sn, payload_size);
    if (client->out.ack_timeout_state == AckTimeoutState_Inactive) {
      prv_reset_ack_timeout(client); // Enable timeout if we don't already have it set
    }
    client->out.next_data_sn = prv_next_sn(sn);
  }
}

// -------------------------------------------------------------------------------------------------

//! Hands a packet to NimBLE, releasing the lock for the call.
static enum pbl_bt_errno prv_write_packet(PPoGATTClient *client, const PPoGATTPacket *packet,
                                          uint16_t length) {
  pbl_mutex_assert_held(&s_lock, true);

  enum pbl_bt_errno e;
#ifdef CONFIG_BT_SERVICE_PPOG_REVERSED
  if (client->role == PPoGATTRoleReversed) {
    prv_unlock();
    e = pbl_bt_ppog_reversed_notify(client->rev.conn_handle, (const uint8_t *)packet, length);
  } else
#endif
  {
    struct pbl_bt_gattc_service *svc = &s_forward_services[client->fwd.instance];
    // If the connection goes away meanwhile, NimBLE fails the conn_handle lookup.
    prv_unlock();
    const int rc =
        pbl_bt_gattc_service_write_no_rsp(svc, PBL_PP_GATT_CHARACTERISTIC_DATA, packet, length);
    if (rc == 0) {
      e = PBL_BT_ERRNO_OK;
    } else if (rc == -ENOMEM) {
      // Usually nothing was sent. Rarely, the first ACL fragments of a PDU that
      // spans several went out; the peer discards that PDU at the next start
      // fragment.
      e = PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES;
    } else {
      e = PBL_BT_ERRNO_INVALID_PARAMETER;
    }
  }
  prv_lock();
  return e;
}

// -------------------------------------------------------------------------------------------------

static void prv_send_next_packets(PPoGATTClient *client) {
  if (client->send_in_progress) {
    // Re-entered while the outer call dropped the lock; reschedule so queued
    // data still gets flushed after the outer call exits.
    if (client->session) {
      pbl_pp_session_send_next(client->session);
    }
    return;
  }
  client->send_in_progress = true;

  uint16_t payload_size = 0;
  const PPoGATTPacket *packet = nullptr;
  PPoGATTPacket *heap_packet = nullptr;

  // Cap the number of times we loop here, to avoid blocking the task for too long.
  uint8_t loop_count = 0;
  while ((packet = prv_prepare_next_packet(client, &heap_packet, &payload_size))) {
    ++loop_count;

    const enum pbl_bt_errno e =
        prv_write_packet(client, packet, sizeof(PPoGATTPacket) + payload_size);
    if (e == PBL_BT_ERRNO_NOT_ENOUGH_RESOURCES) {
      prv_retry_send_later(client);
      break;
    } else if (e != PBL_BT_ERRNO_OK) {
      // Most likely the LE connection got busted, don't think retrying will help.
      PBL_LOG_ERR("Write failed %i", e);
      break;
    }
    if (client->send_retry_count) {
      PBL_LOG_DBG("PPoGATT send recovered after %u retries", client->send_retry_count);
      client->send_retry_count = 0;
    }
    prv_finalize_queued_packet(client, payload_size);

    const uint8_t max_loop_count = 10;
    if (loop_count > max_loop_count) {
      // If more bytes left to send (but loop_count became >= 10),
      // schedule a callback to process them later to avoid blocking the task for too long:
      prv_send_next_packets_async(client);
      break;
    }
  }

  pbl_free(heap_packet);

  client->send_in_progress = false;
}

// -------------------------------------------------------------------------------------------------

static bool prv_session_filter_callback(ListNode *found_node, void *data) {
  return ((const PPoGATTClient *)found_node)->session == data;
}

static PPoGATTClient *prv_find_client_with_session(const struct pbl_pp_session *session) {
  return (PPoGATTClient *)list_find((ListNode *)s_ppogatt_head, prv_session_filter_callback,
                                    (void *)session);
}

static void prv_transport_send_next(struct pbl_pp_session *session) {
  prv_lock();
  PPoGATTClient *client = prv_find_client_with_session(session);
  if (client != nullptr) {
    prv_send_next_packets(client);
  }
  prv_unlock();
}

static void prv_transport_close(struct pbl_pp_session *session) {
  prv_lock();
  PPoGATTClient *client = prv_find_client_with_session(session);
  if (client != nullptr) {
    prv_delete_client(client, false /* is_disconnected */, DeleteReason_CloseCalled);
  }
  prv_unlock();
}

static void prv_transport_reset(struct pbl_pp_session *session) {
  prv_lock();
  PPoGATTClient *client = prv_find_client_with_session(session);
  if (client != nullptr) {
    prv_start_reset(client);
  }
  prv_unlock();
}

static void prv_transport_set_responsiveness(struct pbl_pp_session *session,
                                             enum pbl_bt_consumer consumer,
                                             enum pbl_bt_response_time_state state,
                                             uint16_t max_period_secs) {
  uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

  prv_lock();
  PPoGATTClient *client = prv_find_client_with_session(session);
  if (client != nullptr) {
    conn_handle = prv_get_conn_handle(client);
  }
  prv_unlock();

  if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
    pbl_pp_gatt_cb_set_responsiveness(conn_handle, consumer, state, max_period_secs);
  }
}

static const struct pbl_pp_transport s_transport = {
  .name = "PPoGATT",
  .priority = 2U,
  .send_next = prv_transport_send_next,
  .close = prv_transport_close,
  .reset = prv_transport_reset,
  .set_responsiveness = prv_transport_set_responsiveness,
};

// -------------------------------------------------------------------------------------------------

void pbl_pp_gatt_destroy(void) {
  bool self_initiated_disconnect = false;
  prv_forward_stop();
  prv_lock();
  {
    PPoGATTClient *client = s_ppogatt_head;
    while (client) {
      PPoGATTClient *next = (PPoGATTClient *)client->node.next;
      self_initiated_disconnect = self_initiated_disconnect || client->disconnect_requested;
      prv_delete_client(client, true /* is_disconnected */, DeleteReason_DestroyCalled);
      client = next;
    }
    s_rediscovery_requested_this_connection = false;
  }
  prv_unlock();

  // Only reset the disconnect counter if this was an external disconnect (e.g. RF interference,
  // phone out of range). If we initiated the disconnect ourselves due to max resets, keep the
  // counter so the stall protection can kick in after repeated failures.
  if (!self_initiated_disconnect) {
    pbl_pp_gatt_reset_disconnect_counter();
  }
}

// -------------------------------------------------------------------------------------------------
// For Unit Testing

struct pbl_pp_session *ppogatt_session_for_uuid(const Uuid *uuid) {
  PPoGATTClient *client = prv_find_client_with_uuid(uuid);
  return (client != nullptr) ? client->session : nullptr;
}

enum pbl_pp_destination ppogatt_get_destination_for_uuid(const Uuid *uuid) {
  return prv_find_client_with_uuid(uuid)->destination;
}

const struct pbl_pp_transport *ppogatt_get_transport(void) {
  return &s_transport;
}

bool ppogatt_has_client_for_uuid(const Uuid *uuid) {
  return (prv_find_client_with_uuid(uuid) != nullptr);
}

uint32_t ppogatt_client_count(void) {
  return list_count((ListNode *)s_ppogatt_head);
}

void ppogatt_trigger_rx_ack_send_timeout(void) {
  PPoGATTClient *client = s_ppogatt_head;
  while (client) {
    prv_rx_ack_main_cb(client);
    client = (PPoGATTClient *)client->node.next;
  }
}
