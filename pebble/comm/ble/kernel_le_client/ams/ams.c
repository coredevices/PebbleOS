/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ams.h"

#include "ams_analytics.h"
#include "ams_util.h"

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/bluetooth/clients/ams.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/logging/logging.h>
#include <pbl/services/music_internal.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <comm/ble/gap_le_connection.h>
#include <comm/bt_conn_mgr.h>
#include <comm/bt_lock.h>
#include <kernel/event_loop.h>
#include <kernel/pebble_tasks.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// -------------------------------------------------------------------------------------------------
// Function prototypes

static void prv_perform_on_kernel_main_task(void (*callback)(void *), void *data);

// -------------------------------------------------------------------------------------------------
// Static variables

typedef struct {
  bool connected;
  uint16_t conn_handle;
  //! Whether the service was found: its characteristics can be used.
  bool discovered;
  //! Index in s_registrations.
  uint8_t next_entity_to_register;
  //! Whether a registration awaits its answer.
  bool registering;
} AMSClient;

//! All accesses should happen from KernelMain. Therefore no locking is needed.
static AMSClient *s_ams_client;

PBL_BT_AMS_CLIENT_DEFINE(s_client);

//! No command.
#define COMMAND_INVALID (-1)

static void prv_send_command(int command);

// -------------------------------------------------------------------------------------------------
// MusicServerImplementation

static int prv_ams_command_for_music_command(MusicCommand command) {
  switch (command) {
    case MusicCommandPlay:
      return PBL_BT_AMS_COMMAND_PLAY;
    case MusicCommandPause:
      return PBL_BT_AMS_COMMAND_PAUSE;
    case MusicCommandTogglePlayPause:
      return PBL_BT_AMS_COMMAND_TOGGLE_PLAY_PAUSE;
    case MusicCommandNextTrack:
      return PBL_BT_AMS_COMMAND_NEXT_TRACK;
    case MusicCommandPreviousTrack:
      return PBL_BT_AMS_COMMAND_PREVIOUS_TRACK;
    case MusicCommandVolumeUp:
      return PBL_BT_AMS_COMMAND_VOLUME_UP;
    case MusicCommandVolumeDown:
      return PBL_BT_AMS_COMMAND_VOLUME_DOWN;
    case MusicCommandAdvanceRepeatMode:
      return PBL_BT_AMS_COMMAND_ADVANCE_REPEAT_MODE;
    case MusicCommandAdvanceShuffleMode:
      return PBL_BT_AMS_COMMAND_ADVANCE_SHUFFLE_MODE;
    case MusicCommandSkipForward:
      return PBL_BT_AMS_COMMAND_SKIP_FORWARD;
    case MusicCommandSkipBackward:
      return PBL_BT_AMS_COMMAND_SKIP_BACKWARD;
    case MusicCommandLike:
      return PBL_BT_AMS_COMMAND_LIKE_TRACK;
    case MusicCommandDislike:
      return PBL_BT_AMS_COMMAND_DISLIKE_TRACK;
    case MusicCommandBookmark:
      return PBL_BT_AMS_COMMAND_BOOKMARK_TRACK;
    default:
      return COMMAND_INVALID;
  }
}

static bool prv_music_is_command_supported(MusicCommand command) {
  return (prv_ams_command_for_music_command(command) != COMMAND_INVALID);
}

static void prv_music_command_send(MusicCommand command) {
  const int ams_command = prv_ams_command_for_music_command(command);
  if (ams_command == COMMAND_INVALID) {
    return;
  }
  prv_send_command(ams_command);
}

static MusicServerCapability prv_music_get_capability_bitset(void) {
  return (MusicServerCapabilityPlaybackStateReporting | MusicServerCapabilityProgressReporting |
          MusicServerCapabilityVolumeReporting);
}

static bool prv_music_needs_user_to_start_playback_on_phone(void) {
  return !music_has_now_playing();
}

static void prv_request_response_time(enum pbl_bt_consumer consumer,
                                      enum pbl_bt_response_time_state state,
                                      uint16_t max_period_secs) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);
  bt_lock();
  if (s_ams_client && s_ams_client->discovered) {
    GAPLEConnection *connection = gap_le_connection_any();
    if (connection && connection->conn_handle == s_ams_client->conn_handle) {
      conn_mgr_set_ble_conn_response_time(connection, consumer, state, max_period_secs);
    }
  }
  bt_unlock();
}

static void prv_request_reduced_latency_cb(void *data) {
  const bool reduced_latency = (uintptr_t)data;
  const enum pbl_bt_response_time_state state =
      reduced_latency ? PBL_BT_RESPONSE_TIME_MIDDLE : PBL_BT_RESPONSE_TIME_MAX;
  prv_request_response_time(PBL_BT_CONSUMER_MUSIC_SERVICE_INDEFINITE, state,
                            MAX_PERIOD_RUN_FOREVER);
}

static void prv_request_low_latency_for_period_cb(void *data) {
  const uint32_t period_ms = (uintptr_t)data;
  prv_request_response_time(PBL_BT_CONSUMER_MUSIC_SERVICE_MOMENTARY, PBL_BT_RESPONSE_TIME_MIN,
                            period_ms / PBL_MSEC_PER_SEC);
}

static void prv_music_request_reduced_latency(bool reduced_latency) {
  prv_perform_on_kernel_main_task(prv_request_reduced_latency_cb,
                                  (void *)(uintptr_t)reduced_latency);
}

static void prv_music_request_low_latency_for_period(uint32_t period_ms) {
  prv_perform_on_kernel_main_task(prv_request_low_latency_for_period_cb,
                                  (void *)(uintptr_t)period_ms);
}

static const MusicServerImplementation s_ams_music_implementation = {
  .debug_name = "AMS",
  .is_command_supported = prv_music_is_command_supported,
  .command_send = prv_music_command_send,
  .needs_user_to_start_playback_on_phone = prv_music_needs_user_to_start_playback_on_phone,
  .get_capability_bitset = prv_music_get_capability_bitset,
  .request_reduced_latency = prv_music_request_reduced_latency,
  .request_low_latency_for_period = prv_music_request_low_latency_for_period,
};

// -------------------------------------------------------------------------------------------------
// Internal helpers

static void prv_perform_on_kernel_main_task(void (*callback)(void *), void *data) {
  const bool is_kernel_main = (pebble_task_get_current() == PebbleTask_KernelMain);
  if (is_kernel_main) {
    callback(data);
  } else {
    launcher_task_add_callback(callback, data);
  }
}

static const uint8_t s_player_attrs[] = {
  // Apple bug #21283910
  // http://www.openradar.me/radar?id=6752237204275200
  // Registering for the Player Name attribute can cause BTLEServer to crash repeatedly.
  // (verified in iOS 8.3 and iOS 9 beta 1)
  // PBL_BT_AMS_PLAYER_ATTR_NAME,
  PBL_BT_AMS_PLAYER_ATTR_PLAYBACK_INFO,
  PBL_BT_AMS_PLAYER_ATTR_VOLUME,
};

static const uint8_t s_queue_attrs[] = {
  PBL_BT_AMS_QUEUE_ATTR_INDEX,
  PBL_BT_AMS_QUEUE_ATTR_COUNT,
  PBL_BT_AMS_QUEUE_ATTR_SHUFFLE_MODE,
  PBL_BT_AMS_QUEUE_ATTR_REPEAT_MODE,
};

static const uint8_t s_track_attrs[] = {
  PBL_BT_AMS_TRACK_ATTR_ARTIST,
  PBL_BT_AMS_TRACK_ATTR_ALBUM,
  PBL_BT_AMS_TRACK_ATTR_TITLE,
  PBL_BT_AMS_TRACK_ATTR_DURATION,
};

//! What is registered for, in this order.
static const struct {
  enum pbl_bt_ams_entity entity;
  const uint8_t *attrs;
  uint8_t num_attrs;
} s_registrations[] = {
  {PBL_BT_AMS_ENTITY_PLAYER, s_player_attrs, PBL_ARRAY_SIZE(s_player_attrs)},
  {PBL_BT_AMS_ENTITY_QUEUE, s_queue_attrs, PBL_ARRAY_SIZE(s_queue_attrs)},
  {PBL_BT_AMS_ENTITY_TRACK, s_track_attrs, PBL_ARRAY_SIZE(s_track_attrs)},
};

static void prv_reset_next_entity_to_register(void) {
  s_ams_client->next_entity_to_register = 0;
  s_ams_client->registering = false;
}

static bool prv_is_entity_update_registration_done(void) {
  return (s_ams_client->next_entity_to_register >= PBL_ARRAY_SIZE(s_registrations));
}

static void prv_register_next_entity(void *unused) {
  if (PBL_LIKELY(!s_ams_client || prv_is_entity_update_registration_done())) {
    return;
  }

  // One registration at a time, to keep few operations waiting.
  const uint8_t idx = s_ams_client->next_entity_to_register;
  const int e =
      pbl_bt_ams_client_register(&s_client, s_registrations[idx].entity, s_registrations[idx].attrs,
                                 s_registrations[idx].num_attrs);
  s_ams_client->registering = (e == 0);
  if (e != 0) {
    if (e == -ENOMEM) {
      // Need to wait for space to become available
      launcher_task_add_callback(&prv_register_next_entity, nullptr);
    } else {
      // Most likely the LE connection got busted, don't think retrying will help.
      PBL_LOG_ERR("Write failed %i", e);
    }
  }
}

static bool prv_set_connected(bool connected) {
  if (s_ams_client->connected == connected) {
    return true;
  }
  s_ams_client->connected = connected;
  const bool has_error = !music_set_connected_server(&s_ams_music_implementation, connected);
  if (has_error) {
    s_ams_client->connected = false;
    PBL_LOG_ERR("AMS could not (dis)connect to music service (%u)", connected);
  }
  return !has_error;
}

void ams_music_disconnect(void) {
  if (s_ams_client && s_ams_client->connected) {
    prv_set_connected(false);
  }
}

// -------------------------------------------------------------------------------------------------
// Player entity update handlers

//! The values of the playback info, in the order they come.
enum {
  PlaybackInfoIdxState,
  PlaybackInfoIdxRate,
  PlaybackInfoIdxElapsedTime,
};

static void prv_handle_player_name_update(const struct pbl_bt_ams_update *update) {
  music_update_player_name((const char *)update->value, update->len);
}

static MusicPlayState prv_music_playstate_for_ams_playback_state(int32_t ams_playback_state) {
  switch (ams_playback_state) {
    case PBL_BT_AMS_PLAYBACK_STATE_PAUSED:
      return MusicPlayStatePaused;
    case PBL_BT_AMS_PLAYBACK_STATE_PLAYING:
      return MusicPlayStatePlaying;
    case PBL_BT_AMS_PLAYBACK_STATE_REWINDING:
      return MusicPlayStateRewinding;
    case PBL_BT_AMS_PLAYBACK_STATE_FAST_FORWARDING:
      return MusicPlayStateForwarding;
    default:
      return MusicPlayStateUnknown;
  }
}

static bool prv_handle_player_playback_info_value(const char *value, uint32_t value_length,
                                                  uint32_t idx, void *context) {
  // Non-Apple AMS servers can format the floats with a comma decimal separator
  // (e.g. "1,00"), yielding extra CSV fields. Stop parsing instead of
  // asserting; the caller rejects the update based on the field count.
  if (idx > PlaybackInfoIdxElapsedTime) {
    return false /* should_continue */;
  }

  // Default to -1 for playback state, or 0 otherwise, in case "value" is an empty string:
  // This will cause the playback state to be set to MusicPlayStateUnknown.
  int32_t value_out = (idx == PlaybackInfoIdxState) ? -1 : 0;

  const int32_t multiplier[] = {
    // First value is the playback state, so unity multiplier:
    [PlaybackInfoIdxState] = 1,

    // Second value is the playback rate [0.0, 1.0]. We store as percent, so 100x multiplier:
    [PlaybackInfoIdxRate] = 100,

    // Third value is the elapsed time in seconds. We store as ms, so 1000x multiplier:
    [PlaybackInfoIdxElapsedTime] = 1000,
  };
  if (value_length &&
      !ams_util_float_string_parse(value, value_length, multiplier[idx], &value_out)) {
    PBL_LOG_ERR("AMS playback info value failed to parse: %s", value);
    return false /* should_continue */;
  }

  PBL_LOG_DBG("Playback info value update %" PRId32 "=%" PRId32, idx, value_out);

  MusicPlayerStateUpdate *state = (MusicPlayerStateUpdate *)context;
  switch (idx) {
    case PlaybackInfoIdxState: {
      state->playback_state = prv_music_playstate_for_ams_playback_state(value_out);
      break;
    }

    case PlaybackInfoIdxRate:
      state->playback_rate_percent = value_out;
      break;

    case PlaybackInfoIdxElapsedTime:
      state->elapsed_time_ms = value_out;
      break;

    default:
      pbl_panic();
  }
  return true /* should_continue */;
}

static void prv_handle_player_playback_info_update(const struct pbl_bt_ams_update *update) {
  MusicPlayerStateUpdate state = {};
  const uint8_t num_results = ams_util_csv_parse((const char *)update->value, update->len, &state,
                                                 prv_handle_player_playback_info_value);
  const bool success = (num_results == 3);
  if (success) {
    music_update_player_playback_state(&state);
  } else {
    PBL_LOG_ERR("Expected CSV with 3 values:");
    PBL_LOG_HEXDUMP_ERR(update->value, update->len);
  }
}

static bool prv_float_string_parse(const char *value, const uint16_t value_length,
                                   int32_t multiplier, int32_t *value_in_out) {
  if (value_length && !ams_util_float_string_parse(value, value_length, multiplier, value_in_out)) {
    PBL_LOG_ERR("AMS float failed to parse:");
    PBL_LOG_HEXDUMP_ERR((const uint8_t *)value, value_length);
    return false;
  }
  return true;
}

static void prv_handle_player_volume_update(const struct pbl_bt_ams_update *update) {
  int32_t value_out = 0;
  const bool success =
      prv_float_string_parse((const char *)update->value, update->len, 100, &value_out);
  if (success) {
    music_update_player_volume_percent(value_out);
  }
}

// -------------------------------------------------------------------------------------------------
// Queue entity update handlers

static int32_t prv_parse_queue_value(const char *value, const uint16_t value_length) {
  int32_t value_out = 0;
  prv_float_string_parse(value, value_length, 1, &value_out);
  return value_out;
}

static void prv_handle_queue_index_update(const struct pbl_bt_ams_update *update) {
  const int32_t idx = prv_parse_queue_value((const char *)update->value, update->len);
  PBL_LOG_DBG("Queue index update: %" PRId32, idx);
  // TODO: Do something with this info
}

static void prv_handle_queue_count_update(const struct pbl_bt_ams_update *update) {
  const int32_t count = prv_parse_queue_value((const char *)update->value, update->len);
  PBL_LOG_DBG("Queue count update: %" PRId32, count);
  // TODO: Do something with this info
}

static void prv_handle_queue_shuffle_mode_update(const struct pbl_bt_ams_update *update) {
  const enum pbl_bt_ams_shuffle_mode shuffle_mode =
      prv_parse_queue_value((const char *)update->value, update->len);
  PBL_LOG_DBG("Queue shuffle mode update: %d", shuffle_mode);
  // TODO: Do something with this info
}

static void prv_handle_queue_repeat_mode_update(const struct pbl_bt_ams_update *update) {
  const enum pbl_bt_ams_repeat_mode repeat_mode =
      prv_parse_queue_value((const char *)update->value, update->len);
  PBL_LOG_DBG("Queue repeat mode update: %d", repeat_mode);
  // TODO: Do something with this info
}

// -------------------------------------------------------------------------------------------------
// Track entity update handlers

static void prv_handle_track_artist_update(const struct pbl_bt_ams_update *update) {
  music_update_track_artist((const char *)update->value, update->len);
}

static void prv_handle_track_album_update(const struct pbl_bt_ams_update *update) {
  music_update_track_album((const char *)update->value, update->len);
}

static void prv_handle_track_title_update(const struct pbl_bt_ams_update *update) {
  music_update_track_title((const char *)update->value, update->len);
}

static void prv_handle_track_duration_update(const struct pbl_bt_ams_update *update) {
  int32_t duration_ms = 0; // Default to 0 in case value_length is 0
  const bool success =
      (update->len == 0 || ams_util_float_string_parse((const char *)update->value, update->len,
                                                       PBL_MSEC_PER_SEC, &duration_ms));
  if (success) {
    music_update_track_duration(duration_ms);
  } else {
    PBL_LOG_ERR("AMS duration failed to parse");
  }
}

// -------------------------------------------------------------------------------------------------
// Update handler dispatch table

static void prv_handle_update(const struct pbl_bt_ams_update *update) {
  switch (update->entity) {
    case PBL_BT_AMS_ENTITY_PLAYER:
      switch (update->attr) {
        case PBL_BT_AMS_PLAYER_ATTR_NAME:
          prv_handle_player_name_update(update);
          return;
        case PBL_BT_AMS_PLAYER_ATTR_PLAYBACK_INFO:
          prv_handle_player_playback_info_update(update);
          return;
        case PBL_BT_AMS_PLAYER_ATTR_VOLUME:
          prv_handle_player_volume_update(update);
          return;

        default:
          break;
      }
      break;

    case PBL_BT_AMS_ENTITY_QUEUE:
      switch (update->attr) {
        case PBL_BT_AMS_QUEUE_ATTR_INDEX:
          prv_handle_queue_index_update(update);
          return;
        case PBL_BT_AMS_QUEUE_ATTR_COUNT:
          prv_handle_queue_count_update(update);
          return;
        case PBL_BT_AMS_QUEUE_ATTR_SHUFFLE_MODE:
          prv_handle_queue_shuffle_mode_update(update);
          return;
        case PBL_BT_AMS_QUEUE_ATTR_REPEAT_MODE:
          prv_handle_queue_repeat_mode_update(update);
          return;

        default:
          break;
      }
      break;

    case PBL_BT_AMS_ENTITY_TRACK:
      switch (update->attr) {
        case PBL_BT_AMS_TRACK_ATTR_ARTIST:
          prv_handle_track_artist_update(update);
          return;
        case PBL_BT_AMS_TRACK_ATTR_ALBUM:
          prv_handle_track_album_update(update);
          return;
        case PBL_BT_AMS_TRACK_ATTR_TITLE:
          prv_handle_track_title_update(update);
          // New track: reset the position until the next playback info update.
          music_update_track_position(0);
          return;
        case PBL_BT_AMS_TRACK_ATTR_DURATION:
          prv_handle_track_duration_update(update);
          return;

        default:
          break;
      }
      break;

    default:
      break;
  }

  PBL_LOG_ERR("Unknown EntityID:%u + AttrID:%u", update->entity, update->attr);
}

// -------------------------------------------------------------------------------------------------
// What the client reports, on KernelMain

static void prv_invalidate_all_references(void) {
  // The service is gone, and with it the subscription, so we're disconnected from AMS:
  prv_set_connected(false);

  // We also need to register for entity updates again:
  prv_reset_next_entity_to_register();

  s_ams_client->discovered = false;
}

static void prv_handle_service_discovered(void) {
  s_ams_client->discovered = true;

  // Reject the service if subscribing fails instead of asserting.
  const int e = pbl_bt_ams_client_subscribe(&s_client, PBL_BT_AMS_SOURCE_ENTITY_UPDATE, true);
  if (e != 0) {
    PBL_LOG_WRN("Failed to subscribe AMS (err=%d), ignoring service", e);
    prv_invalidate_all_references();
  }
}

static void prv_handle_subscribed(int status) {
  if (status != 0) {
    PBL_LOG_ERR("Failed to subscribe AMS");
    return;
  }
  PBL_LOG_INFO("AMS subscribed");
  if (!prv_set_connected(true)) {
    PBL_LOG_ERR("Another music service was already connected. Aborting AMS setup.");
    return;
  }
  prv_register_next_entity(nullptr);
}

static void prv_handle_registered(int status) {
  // An answer from before the service moved is not for the registration under way.
  if (!s_ams_client->registering) {
    return;
  }
  s_ams_client->registering = false;
  const uint8_t idx = s_ams_client->next_entity_to_register;
  if (status != 0) {
    PBL_LOG_ERR("AMS Failed to register entity_id=%u: %d", s_registrations[idx].entity, status);
    // Don't retry here, chances of succeeding are slim.
    return;
  }
  PBL_LOG_DBG("AMS Registered for entity_id=%u", s_registrations[idx].entity);
  ++s_ams_client->next_entity_to_register;
  prv_register_next_entity(nullptr);
}

// -------------------------------------------------------------------------------------------------
// The client calls back on the host task; the session runs on KernelMain.

typedef enum {
  ClientEventDiscovered,
  ClientEventSubscribed,
  ClientEventRegistered,
  ClientEventUpdate,
} ClientEventType;

typedef struct {
  ClientEventType type;
  uint32_t session;
  int status;
  struct pbl_bt_ams_update update;
  uint8_t value[];
} ClientEvent;

//! At most this many events wait for KernelMain; updates past it are dropped.
#define CLIENT_EVENTS_MAX 32

//! Tells the events of a session that ended from those of the current one.
static uint32_t s_session;
static atomic_uint s_num_client_events;

static void prv_client_event_kernel_main_cb(void *data) {
  ClientEvent *event = data;

  atomic_fetch_sub(&s_num_client_events, 1);

  if (s_ams_client && event->session == s_session) {
    switch (event->type) {
      case ClientEventDiscovered:
        // The service moved or went, if it was found before.
        if (s_ams_client->discovered) {
          prv_invalidate_all_references();
        }
        if (event->status == 0) {
          prv_handle_service_discovered();
        } else {
          PBL_LOG_DBG("No AMS (%d)", event->status);
        }
        break;
      case ClientEventSubscribed:
        prv_handle_subscribed(event->status);
        break;
      case ClientEventRegistered:
        prv_handle_registered(event->status);
        break;
      case ClientEventUpdate:
        if (s_ams_client->discovered) {
          prv_handle_update(&event->update);
        }
        break;
    }
  }

  pbl_free(event);
}

static void prv_put_client_event(ClientEventType type, int status,
                                 const struct pbl_bt_ams_update *update) {
  if (atomic_fetch_add(&s_num_client_events, 1) >= CLIENT_EVENTS_MAX && type == ClientEventUpdate) {
    atomic_fetch_sub(&s_num_client_events, 1);
    PBL_LOG_WRN("Dropping AMS update");
    return;
  }

  // On the host task: an event is lost rather than the watch reset when memory is short.
  const uint16_t length = (update != nullptr) ? update->len : 0;
  // The value is terminated for the parsers, which look one character ahead.
  ClientEvent *event = pbl_malloc(sizeof(*event) + length + 1);
  if (event == nullptr) {
    atomic_fetch_sub(&s_num_client_events, 1);
    PBL_LOG_WRN("Dropping AMS event %d: out of memory", type);
    return;
  }
  *event = (ClientEvent){
    .type = type,
    .session = s_session,
    .status = status,
  };
  if (update != nullptr) {
    event->update = *update;
    event->update.value = event->value;
    memcpy(event->value, update->value, length);
    event->value[length] = '\0';
  }
  launcher_task_add_callback(prv_client_event_kernel_main_cb, event);
}

static void prv_client_discovered(struct pbl_bt_ams_client *client, int status) {
  prv_put_client_event(ClientEventDiscovered, status, nullptr);
}

static void prv_client_subscribed(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
                                  bool enable, int status) {
  if (enable && source == PBL_BT_AMS_SOURCE_ENTITY_UPDATE) {
    prv_put_client_event(ClientEventSubscribed, status, nullptr);
  }
}

static void prv_client_update(struct pbl_bt_ams_client *client,
                              const struct pbl_bt_ams_update *update) {
  prv_put_client_event(ClientEventUpdate, 0, update);
}

static void prv_client_registered(struct pbl_bt_ams_client *client, int status) {
  prv_put_client_event(ClientEventRegistered, status, nullptr);
}

static const struct pbl_bt_ams_client_cb s_client_cb = {
  .discovered = prv_client_discovered,
  .subscribed = prv_client_subscribed,
  .update = prv_client_update,
  .registered = prv_client_registered,
};

// -------------------------------------------------------------------------------------------------
// Interface towards kernel_le_client.c

void ams_create(uint16_t conn_handle) {
  PBL_ASSERTN(!s_ams_client);
  s_ams_client = (AMSClient *)pbl_zalloc_check(sizeof(AMSClient));
  s_ams_client->conn_handle = conn_handle;

  ++s_session;
  const int rc = pbl_bt_ams_client_start(&s_client, conn_handle, &s_client_cb);
  if (rc != 0) {
    PBL_LOG_ERR("Could not look for AMS (%d)", rc);
  }
}

void ams_handle_services_changed(void) {
  if (!s_ams_client) {
    return;
  }

  // The session goes on if the service is still where it was.
  const int rc = pbl_bt_ams_client_rediscover(&s_client);
  if (rc != 0) {
    PBL_LOG_ERR("Could not look for AMS again (%d)", rc);
  }
}

void ams_destroy(void) {
  if (!s_ams_client) {
    return;
  }
  pbl_bt_ams_client_stop(&s_client);
  ++s_session;
  prv_set_connected(false);

  pbl_free(s_ams_client);
  s_ams_client = nullptr;
}

static void prv_send_command_kernel_main_task_cb(void *data) {
  if (!s_ams_client) {
    return;
  }
  const int error = pbl_bt_ams_client_send_command(&s_client, (uintptr_t)data);
  if (error != 0) {
    PBL_LOG_ERR("Couldn't write command: %d", error);
  }
}

static void prv_send_command(int command) {
  prv_perform_on_kernel_main_task(prv_send_command_kernel_main_task_cb, (void *)(uintptr_t)command);
}

const char *ams_music_server_debug_name(void) {
  return s_ams_music_implementation.debug_name;
}

bool ams_is_registered_for_all_entity_updates(void) {
  if (!s_ams_client) {
    return false;
  }
  return prv_is_entity_update_registration_done();
}
