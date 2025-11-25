/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ams.h"
#include "ams_analytics.h"
#include "ams_util.h"

#include "comm/ble/ble_log.h"
#include "comm/ble/gap_le_connection.h"
#include "comm/ble/gatt_client_accessors.h"
#include "comm/ble/gatt_client_operations.h"
#include "comm/ble/gatt_client_subscriptions.h"
#include "comm/bt_conn_mgr.h"
#include "comm/bt_lock.h"

#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"

#include "services/common/analytics/analytics_event.h"
#include "services/normal/music_internal.h"

#include "system/logging.h"
#include "system/hexdump.h"
#include "system/passert.h"
#include "util/likely.h"
#include "util/time/time.h"

#include <btutil/bt_device.h>

#include <string.h>

// -------------------------------------------------------------------------------------------------
// Function prototypes

static void prv_perform_on_kernel_main_task(void (*callback)(void *), void *data);

// -------------------------------------------------------------------------------------------------
// Static variables

typedef struct {
  bool connected;
  BLECharacteristic characteristics[NumAMSCharacteristic];
  AMSEntityID next_entity_to_register;
} AMSClient;

//! All accesses should happen from KernelMain. Therefore no locking is needed.
static AMSClient *s_ams_client;

// -------------------------------------------------------------------------------------------------
// MusicServerImplementation

static AMSRemoteCommandID prv_ams_command_for_music_command(MusicCommand command) {
  switch (command) {
    case MusicCommandPlay:
      return AMSRemoteCommandIDPlay;
    case MusicCommandPause:
      return AMSRemoteCommandIDPause;
    case MusicCommandTogglePlayPause:
      return AMSRemoteCommandIDTogglePlayPause;
    case MusicCommandNextTrack:
      return AMSRemoteCommandIDNextTrack;
    case MusicCommandPreviousTrack:
      return AMSRemoteCommandIDPreviousTrack;
    case MusicCommandVolumeUp:
      return AMSRemoteCommandIDVolumeUp;
    case MusicCommandVolumeDown:
      return AMSRemoteCommandIDVolumeDown;
    case MusicCommandAdvanceRepeatMode:
      return AMSRemoteCommandIDAdvanceRepeatMode;
    case MusicCommandAdvanceShuffleMode:
      return AMSRemoteCommandIDAdvanceShuffleMode;
    case MusicCommandSkipForward:
      return AMSRemoteCommandIDSkipForward;
    case MusicCommandSkipBackward:
      return AMSRemoteCommandIDSkipBackward;
    case MusicCommandLike:
      return AMSRemoteCommandIDLike;
    case MusicCommandDislike:
      return AMSRemoteCommandIDDislike;
    case MusicCommandBookmark:
      return AMSRemoteCommandIDBookmark;
    default:
      return AMSRemoteCommandIDInvalid;
  }
}

static bool prv_music_is_command_supported(MusicCommand command) {
  return (prv_ams_command_for_music_command(command) != AMSRemoteCommandIDInvalid);
}

static void prv_music_command_send(MusicCommand command) {
  const AMSRemoteCommandID ams_command = prv_ams_command_for_music_command(command);
  if (ams_command == AMSRemoteCommandIDInvalid) {
    return;
  }
  ams_send_command(ams_command);
}

static MusicServerCapability prv_music_get_capability_bitset(void) {
  return (MusicServerCapabilityPlaybackStateReporting |
          MusicServerCapabilityProgressReporting |
          MusicServerCapabilityVolumeReporting);
}

static bool prv_music_needs_user_to_start_playback_on_phone(void) {
  return !music_has_now_playing();
}

static void prv_request_response_time(BtConsumer consumer, ResponseTimeState state,
                                      uint16_t max_period_secs) {
  PBL_ASSERT_TASK(PebbleTask_KernelMain);
  bt_lock();
  if (s_ams_client) {
    const BLECharacteristic characteristic = s_ams_client->characteristics[0];
    const BTDeviceInternal device = gatt_client_characteristic_get_device(characteristic);
    if (!bt_device_is_invalid(&device.opaque)) {
      GAPLEConnection *connection = gap_le_connection_by_device(&device);
      conn_mgr_set_ble_conn_response_time(connection, consumer, state, max_period_secs);
    }
  }
  bt_unlock();
}

static void prv_request_reduced_latency_cb(void *data) {
  const bool reduced_latency = (uintptr_t)data;
  const ResponseTimeState state = reduced_latency ? ResponseTimeMiddle : ResponseTimeMax;
  prv_request_response_time(BtConsumerMusicServiceIndefinite, state, MAX_PERIOD_RUN_FOREVER);
}

static void prv_request_low_latency_for_period_cb(void *data) {
  const uint32_t period_ms = (uintptr_t)data;
  prv_request_response_time(BtConsumerMusicServiceMomentary, ResponseTimeMin,
                            period_ms / MS_PER_SECOND);
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

static void prv_analytics_log_event_with_info(AMSAnalyticsEvent event, int32_t aux_info) {
  analytics_event_ams(event, aux_info);
}

static void prv_perform_on_kernel_main_task(void (*callback)(void *), void *data) {
  const bool is_kernel_main = (pebble_task_get_current() == PebbleTask_KernelMain);
  if (is_kernel_main) {
    callback(data);
  } else {
    launcher_task_add_callback(callback, data);
  }
}

static AMSCharacteristic prv_get_id_for_characteristic(BLECharacteristic characteristic_to_find) {
  if (!s_ams_client) {
    return AMSCharacteristicInvalid;
  }
  const BLECharacteristic *characteristic = s_ams_client->characteristics;
  for (AMSCharacteristic id = 0; id < NumAMSCharacteristic; ++id, ++characteristic) {
    if (*characteristic == characteristic_to_find) {
      return id;
    }
  }
  return AMSCharacteristicInvalid;
}

static const uint8_t *prv_get_registration_cmd_for_entity(AMSEntityID entity_id,
                                                          uint8_t *cmd_length_out) {
  static const uint8_t register_for_player_entity_updates_cmd[] = {
    AMSEntityIDPlayer,
    // Apple bug #21283910
    // http://www.openradar.me/radar?id=6752237204275200
    // Registering for the Player Name attribute can cause BTLEServer to crash repeatedly.
    // (verified in iOS 8.3 and iOS 9 beta 1)
    // AMSPlayerAttributeIDName,
    AMSPlayerAttributeIDPlaybackInfo,
    AMSPlayerAttributeIDVolume,
  };
  static const uint8_t register_for_queue_entity_updates_cmd[] = {
    AMSEntityIDQueue,
    AMSQueueAttributeIDIndex,
    AMSQueueAttributeIDCount,
    AMSQueueAttributeIDShuffleMode,
    AMSQueueAttributeIDRepeatMode,
  };
  static const uint8_t register_for_track_entity_updates_cmd[] = {
    AMSEntityIDTrack,
    AMSTrackAttributeIDArtist,
    AMSTrackAttributeIDAlbum,
    AMSTrackAttributeIDTitle,
    AMSTrackAttributeIDDuration,
  };
  static const struct {
    const uint8_t length;
    const uint8_t * const value;
  } packet_length_and_data[NumAMSEntityID] = {
    [AMSEntityIDPlayer] = {
      .length = sizeof(register_for_player_entity_updates_cmd),
      .value = register_for_player_entity_updates_cmd,
    },
    [AMSEntityIDQueue] = {
      .length = sizeof(register_for_queue_entity_updates_cmd),
      .value = register_for_queue_entity_updates_cmd,
    },
    [AMSEntityIDTrack] = {
      .length = sizeof(register_for_track_entity_updates_cmd),
      .value = register_for_track_entity_updates_cmd,
    },
  };
  if (cmd_length_out) {
    *cmd_length_out = packet_length_and_data[entity_id].length;
  }
  return packet_length_and_data[entity_id].value;
}

static void prv_reset_next_entity_to_register(void) {
  s_ams_client->next_entity_to_register = AMSEntityIDPlayer;
}

static bool prv_is_entity_update_registration_done(void) {
  return (s_ams_client->next_entity_to_register == AMSEntityIDInvalid);
}

static void prv_register_next_entity(void *unused) {
  if (LIKELY(!s_ams_client || prv_is_entity_update_registration_done())) {
    return;
  }

  // Make the Bluetopia heap consumption of this module as minimal and predictable as possible,
  // by having only one outstanding GATT operation queued up at any moment in time (instead of
  // queueing up all the writes in one go):
  const AMSEntityID entity_id = s_ams_client->next_entity_to_register;
  const BLECharacteristic entity_update_characteristic =
                            s_ams_client->characteristics[AMSCharacteristicEntityUpdate];
  uint8_t cmd_length = 0;
  const uint8_t *cmd_value = prv_get_registration_cmd_for_entity(entity_id, &cmd_length);
  const BTErrno e = gatt_client_op_write(entity_update_characteristic,
                                         cmd_value, cmd_length,
                                         GAPLEClientKernel);
  if (e != BTErrnoOK) {
    if (e == BTErrnoNotEnoughResources) {
      // Need to wait for space to become available
      launcher_task_add_callback(&prv_register_next_entity, NULL);
    } else {
      // Most likely the LE connection got busted, don't think retrying will help.
      PBL_LOG(LOG_LEVEL_ERROR, "Write failed %i", e);
    }
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorRegisterEntityWrite, e);
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
    PBL_LOG(LOG_LEVEL_ERROR, "AMS could not (dis)connect to music service (%u)", connected);
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorMusicServiceConnect, connected ? 1 : 2);
  }
  return !has_error;
}

// -------------------------------------------------------------------------------------------------
// Player entity update handlers

static void prv_handle_player_name_update(const AMSEntityUpdateNotification *update,
                                          const uint16_t value_length) {
  music_update_player_name(update->value_str, value_length);
}

static MusicPlayState prv_music_playstate_for_ams_playback_state(int32_t ams_playback_state) {
  switch (ams_playback_state) {
    case AMSPlaybackStatePaused: return MusicPlayStatePaused;
    case AMSPlaybackStatePlaying: return MusicPlayStatePlaying;
    case AMSPlaybackStateRewinding: return MusicPlayStateRewinding;
    case AMSPlaybackStateForwarding: return MusicPlayStateForwarding;
    default: return MusicPlayStateUnknown;
  }
}

static bool prv_handle_player_playback_info_value(const char *value, uint32_t value_length,
                                                  uint32_t idx, void *context) {
  // Default to -1 for playback state, or 0 otherwise, in case "value" is an empty string:
  // This will cause the playback state to be set to MusicPlayStateUnknown.
  int32_t value_out = (idx == AMSPlaybackInfoIdxState) ? -1 : 0;

  const int32_t multiplier[] = {
    // First value is the AMSPlaybackState enum, so unity multiplier:
    [AMSPlaybackInfoIdxState] = 1,

    // Second value is the playback rate [0.0, 1.0]. We store as percent, so 100x multiplier:
    [AMSPlaybackInfoIdxRate] = 100,

    // Third value is the elapsed time in seconds. We store as ms, so 1000x multiplier:
    [AMSPlaybackInfoIdxElapsedTime] = 1000,
  };
  if (value_length && !ams_util_float_string_parse(value, value_length,
                                                   multiplier[idx], &value_out)) {
    PBL_LOG(LOG_LEVEL_ERROR, "AMS playback info value failed to parse: %s", value);
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorPlayerPlaybackInfoFloatParse, idx);
    return false /* should_continue */;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Playback info value update %"PRId32"=%"PRId32, idx, value_out);

  MusicPlayerStateUpdate *state = (MusicPlayerStateUpdate *)context;
  switch (idx) {
    case AMSPlaybackInfoIdxState: {
      state->playback_state = prv_music_playstate_for_ams_playback_state(value_out);
      break;
    }

    case AMSPlaybackInfoIdxRate:
      state->playback_rate_percent = value_out;
      break;

    case AMSPlaybackInfoIdxElapsedTime:
      state->elapsed_time_ms = value_out;
      break;

    default:
      WTF;
  }
  return true /* should_continue */;
}

static void prv_handle_player_playback_info_update(const AMSEntityUpdateNotification *update,
                                                   const uint16_t value_length) {
  MusicPlayerStateUpdate state = {};
  const uint8_t num_results = ams_util_csv_parse(update->value_str, value_length, &state,
                                                 prv_handle_player_playback_info_value);
  const bool success = (num_results == 3);
  if (success) {
    music_update_player_playback_state(&state);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Expected CSV with 3 values:");
    PBL_HEXDUMP(LOG_LEVEL_ERROR, (const uint8_t *) update->value_str, value_length);
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorPlayerPlaybackInfoUpdate, num_results);
  }
}

static bool prv_float_string_parse(const char *value, const uint16_t value_length,
                                   int32_t multiplier, int32_t *value_in_out) {
  if (value_length &&
      !ams_util_float_string_parse(value, value_length, multiplier, value_in_out)) {
    PBL_LOG(LOG_LEVEL_ERROR, "AMS float failed to parse:");
    PBL_HEXDUMP(LOG_LEVEL_ERROR, (const uint8_t *)value, value_length);
    return false;
  }
  return true;
}

static void prv_handle_player_volume_update(const AMSEntityUpdateNotification *update,
                                            const uint16_t value_length) {
  int32_t value_out = 0;
  const bool success = prv_float_string_parse(update->value_str, value_length, 100, &value_out);
  if (success) {
    music_update_player_volume_percent(value_out);
  } else {
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorPlayerVolumeUpdate, value_length);
  }
}

// -------------------------------------------------------------------------------------------------
// Queue entity update handlers

static int32_t prv_parse_queue_value(const char *value, const uint16_t value_length) {
  int32_t value_out = 0;
  prv_float_string_parse(value, value_length, 1, &value_out);
  return value_out;
}

static void prv_handle_queue_index_update(const AMSEntityUpdateNotification *update,
                                          const uint16_t value_length) {
  const int32_t idx = prv_parse_queue_value(update->value_str, value_length);
  PBL_LOG(LOG_LEVEL_DEBUG, "Queue index update: %"PRId32, idx);
  // TODO: Do something with this info
}

static void prv_handle_queue_count_update(const AMSEntityUpdateNotification *update,
                                          const uint16_t value_length) {
  const int32_t count = prv_parse_queue_value(update->value_str, value_length);
  PBL_LOG(LOG_LEVEL_DEBUG, "Queue count update: %"PRId32, count);
  // TODO: Do something with this info
}

static void prv_handle_queue_shuffle_mode_update(const AMSEntityUpdateNotification *update,
                                                 const uint16_t value_length) {
  const AMSShuffleMode shuffle_mode = prv_parse_queue_value(update->value_str, value_length);
  PBL_LOG(LOG_LEVEL_DEBUG, "Queue shuffle mode update: %d", shuffle_mode);
  // TODO: Do something with this info
}

static void prv_handle_queue_repeat_mode_update(const AMSEntityUpdateNotification *update,
                                                const uint16_t value_length) {
  const AMSRepeatMode repeat_mode = prv_parse_queue_value(update->value_str, value_length);
  PBL_LOG(LOG_LEVEL_DEBUG, "Queue repeat mode update: %d", repeat_mode);
  // TODO: Do something with this info
}

// -------------------------------------------------------------------------------------------------
// Track entity update handlers

static void prv_handle_track_artist_update(const AMSEntityUpdateNotification *update,
                                          const uint16_t value_length) {
  music_update_track_artist(update->value_str, value_length);
}

static void prv_handle_track_album_update(const AMSEntityUpdateNotification *update,
                                           const uint16_t value_length) {
  music_update_track_album(update->value_str, value_length);
}

static void prv_handle_track_title_update(const AMSEntityUpdateNotification *update,
                                          const uint16_t value_length) {
  music_update_track_title(update->value_str, value_length);
}

static void prv_handle_track_duration_update(const AMSEntityUpdateNotification *update,
                                             const uint16_t value_length) {
  int32_t duration_ms = 0;  // Default to 0 in case value_length is 0
  const bool success =
      (!value_length ||
       ams_util_float_string_parse(update->value_str, value_length, MS_PER_SECOND, &duration_ms));
  if (success) {
    music_update_track_duration(duration_ms);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "AMS duration failed to parse");
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorTrackDurationUpdate, value_length);
  }
}

// -------------------------------------------------------------------------------------------------
// Update handler dispatch table

typedef void (*AMSUpdateHandler)(const AMSEntityUpdateNotification *update,
                                 const uint16_t value_length);

static void prv_handle_update(const AMSEntityUpdateNotification *update,
                              const uint16_t value_length) {
  switch (update->entity_id) {
    case AMSEntityIDPlayer:
      switch (update->attribute_id) {
        case AMSPlayerAttributeIDName:
          prv_handle_player_name_update(update, value_length);
          return;
        case AMSPlayerAttributeIDPlaybackInfo:
          prv_handle_player_playback_info_update(update, value_length);
          return;
        case AMSPlayerAttributeIDVolume:
          prv_handle_player_volume_update(update, value_length);
          return;

        default:
          break;
      }
      break;

    case AMSEntityIDQueue:
      switch (update->attribute_id) {
        case AMSQueueAttributeIDIndex:
          prv_handle_queue_index_update(update, value_length);
          return;
        case AMSQueueAttributeIDCount:
          prv_handle_queue_count_update(update, value_length);
          return;
        case AMSQueueAttributeIDShuffleMode:
          prv_handle_queue_shuffle_mode_update(update, value_length);
          return;
        case AMSQueueAttributeIDRepeatMode:
          prv_handle_queue_repeat_mode_update(update, value_length);
          return;

        default:
          break;
      }
      break;

    case AMSEntityIDTrack:
      switch (update->attribute_id) {
        case AMSTrackAttributeIDArtist:
          prv_handle_track_artist_update(update, value_length);
          return;
        case AMSTrackAttributeIDAlbum:
          prv_handle_track_album_update(update, value_length);
          return;
        case AMSTrackAttributeIDTitle:
          prv_handle_track_title_update(update, value_length);
          // FIXME: This is a workaround. See PBL-21818
          music_update_track_position(0);
          return;
        case AMSTrackAttributeIDDuration:
          prv_handle_track_duration_update(update, value_length);
          return;

        default:
          break;
      }
      break;

    default:
      break;
  }

  PBL_LOG(LOG_LEVEL_ERROR, "Unknown EntityID:%u + AttrID:%u",
          update->entity_id, update->attribute_id);
}

// -------------------------------------------------------------------------------------------------
// Interface towards kernel_le_client.c

void ams_create(void) {
  PBL_ASSERTN(!s_ams_client);
  s_ams_client = (AMSClient *) kernel_zalloc_check(sizeof(AMSClient));
}

void ams_invalidate_all_references(void) {
  // We've gotten new characteristic references,
  // this means the old ones will have been unsubscribed, so we're disconnected from AMS:
  prv_set_connected(false);

  // We also need to register for entity updates again:
  prv_reset_next_entity_to_register();

  for (int c = 0; c < NumAMSCharacteristic; c++) {
    s_ams_client->characteristics[c] = BLE_CHARACTERISTIC_INVALID;
  }
}

void ams_handle_service_removed(BLECharacteristic *characteristics, uint8_t num_characteristics) {
  ams_invalidate_all_references();
}

void ams_handle_service_discovered(BLECharacteristic *characteristics) {
  if (!s_ams_client) {
    return;
  }

  BLE_LOG_DEBUG("In AMS service discovery CB");
  PBL_ASSERTN(characteristics);

  if (s_ams_client->characteristics[0] != BLE_CHARACTERISTIC_INVALID) {
    PBL_LOG(LOG_LEVEL_WARNING, "Multiple AMS instances registered!?");
    return;
  }

  // Keep around the BLECharacteristic references:
  memcpy(s_ams_client->characteristics, characteristics,
         sizeof(BLECharacteristic) * NumAMSCharacteristic);

  const BLECharacteristic entity_update_characteristic =
      characteristics[AMSCharacteristicEntityUpdate];
  const BTErrno e = gatt_client_subscriptions_subscribe(entity_update_characteristic,
                                                        BLESubscriptionNotifications,
                                                        GAPLEClientKernel);
  PBL_ASSERTN(e == BTErrnoOK);
}

bool ams_can_handle_characteristic(BLECharacteristic characteristic) {
  if (!s_ams_client) {
    return false;
  }
  for (int c = 0; c < NumAMSCharacteristic; ++c) {
    if (s_ams_client->characteristics[c] == characteristic) {
      return true;
    }
  }
  return false;
}

void ams_handle_subscribe(BLECharacteristic subscribed_characteristic,
                          BLESubscription subscription_type, BLEGATTError error) {
  AMSCharacteristic characteristic_id = prv_get_id_for_characteristic(subscribed_characteristic);
  if (characteristic_id != AMSCharacteristicEntityUpdate) {
    // Only Entity Update characteristic is expected to be subscribed to
    WTF;
  }

  if (error != BLEGATTErrorSuccess) {
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorSubscribe, error);
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to subscribe AMS");
    return;
  }
  PBL_LOG(LOG_LEVEL_INFO, "Hurray! AMS subscribed");
  if (!prv_set_connected(true)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Another music service was already connected. Aborting AMS setup.");
    return;
  }
  prv_register_next_entity(NULL);
}

void ams_handle_write_response(BLECharacteristic characteristic, BLEGATTError error) {
  if (!s_ams_client) {
    return;
  }
  const bool is_entity_update_characteristic =
      (characteristic == s_ams_client->characteristics[AMSCharacteristicEntityUpdate]);

  const bool has_error = (error != BLEGATTErrorSuccess);
  if (has_error) {
    const AMSAnalyticsEvent event = is_entity_update_characteristic ?
                                    AMSAnalyticsEventErrorRegisterEntityWriteResponse :
                                    AMSAnalyticsEventErrorOtherWriteResponse;
    prv_analytics_log_event_with_info(event, error);
  }

  if (!is_entity_update_characteristic) {
    // We only need to act upon getting a write response of the Entity Update characteristic.
    // Just ignore write responses for the Remote Command characteristic.
    return;
  }
  const AMSEntityID entity_id = s_ams_client->next_entity_to_register;
  if (has_error) {
    PBL_LOG(LOG_LEVEL_ERROR, "AMS Failed to register entity_id=%u: %u", entity_id, error);
    // TODO: Log error event
    // Don't retry here, chances of succeeding are slim.
    return;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "AMS Registered for entity_id=%u", entity_id);
  ++s_ams_client->next_entity_to_register;
  prv_register_next_entity(NULL);
}

void ams_handle_read_or_notification(BLECharacteristic characteristic, const uint8_t *value,
                                     size_t value_length, BLEGATTError error) {
  if (!s_ams_client ||
      s_ams_client->characteristics[AMSCharacteristicEntityUpdate] != characteristic) {
    PBL_LOG(LOG_LEVEL_ERROR, "Unexpected characteristic (s_ams_client=%p)", s_ams_client);
    return;
  }
  PBL_HEXDUMP(LOG_LEVEL_DEBUG, value, value_length);
  const AMSEntityUpdateNotification *update = (const AMSEntityUpdateNotification *)value;
  prv_handle_update(update, value_length - sizeof(*update));
}

void ams_destroy(void) {
  if (!s_ams_client) {
    return;
  }
  prv_set_connected(false);

  kernel_free(s_ams_client);
  s_ams_client = NULL;
}

static void prv_send_command_kernel_main_task_cb(void *data) {
  if (!s_ams_client) {
    return;
  }
  const AMSRemoteCommandID command_id = (uintptr_t)data;
  BLECharacteristic characteristic = s_ams_client->characteristics[AMSCharacteristicRemoteCommand];
  BTErrno error = gatt_client_op_write(characteristic,
                                       (const uint8_t *) &command_id, 1, GAPLEClientKernel);
  const bool has_error = (error != BTErrnoOK);
  if (has_error) {
    PBL_LOG(LOG_LEVEL_ERROR, "Couldn't write command: %d", error);
    prv_analytics_log_event_with_info(AMSAnalyticsEventErrorSendRemoteCommand, error);
  }
}

void ams_send_command(AMSRemoteCommandID command_id) {
  prv_perform_on_kernel_main_task(prv_send_command_kernel_main_task_cb,
                                  (void *)(uintptr_t)command_id);
}

const char *ams_music_server_debug_name(void) {
  return s_ams_music_implementation.debug_name;
}

bool ams_is_registered_for_all_entity_updates(void) {
  if (!s_ams_client) {
    return false;
  }
  return (s_ams_client->next_entity_to_register == AMSEntityIDInvalid);
}
