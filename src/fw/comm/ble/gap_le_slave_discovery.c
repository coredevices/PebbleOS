/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "board/board.h"

#include "gap_le_slave_discovery.h"
#include "gap_le_advert.h"

#include "applib/bluetooth/ble_ad_parse.h"

#include "comm/ble/ble_log.h"
#include "comm/bt_lock.h"

#include "git_version.auto.h"

#include "mfg/mfg_info.h"

#include "mfg/mfg_serials.h"

#include "services/common/bluetooth/local_id.h"
#include "services/normal/bluetooth/ble_hrm.h"

#include "system/passert.h"
#include "system/version.h"

#include <bluetooth/pebble_bt.h>
#include <bluetooth/pebble_pairing_service.h>
#include <bluetooth/bluetooth_types.h>
#include <btutil/bt_uuid.h>
#include <util/attributes.h>
#include <util/size.h>

static GAPLEAdvertisingJobRef s_discovery_advert_job;

// -----------------------------------------------------------------------------
//! Handles unscheduling of the discovery advertisement job.
static void prv_job_unschedule_callback(GAPLEAdvertisingJobRef job,
                                        bool completed,
                                        void *cb_data) {
  // Cleanup:
  s_discovery_advert_job = NULL;
}

// -----------------------------------------------------------------------------
//! Schedules the discovery advertisement job.
//! We don't want to be advertising at a high rate infinitely. When duration
//! is 0, a short period of high-rate advertising will be used. When this short
//! period is completed, an indefinite, low-rate job will be scheduled.
static void prv_schedule_ad_job(void) {
  BLEAdData *ad = ble_ad_create();

  // Advertisement part:
  // Centrals will be filtering on Service UUID first. Assuming that the
  // central is only doing a scan request if the Service UUID matches with their
  // interests, to save radio time / battery life we keep the advertisement part
  // as "small" as possible (21 bytes currently).
  ble_ad_set_flags(ad, GAP_LE_AD_FLAGS_GEN_DISCOVERABLE_MASK);

  // *DO NOT* use pebble_bt_uuid_expand() here!
  // ble_ad_set_service_uuids() will be "smart" and include only the 16-bit UUID, but only if the
  // BT SIG Base UUID is used.
  Uuid service_uuids[2];
  size_t num_uuids = 0;

#if CAPABILITY_HAS_BUILTIN_HRM
  // NOTE: The HRM service has to be first in the list because otherwise the Pebble won't
  // show up as an HRM device in Strava for Android...
  if (ble_hrm_is_supported_and_enabled()) {
    service_uuids[num_uuids++] = bt_uuid_expand_16bit(0x180D);  // Heart Rate Service
  }
#endif

  // Pebble Pairing Service UUID:
  service_uuids[num_uuids++] = bt_uuid_expand_16bit(PEBBLE_BT_PAIRING_SERVICE_UUID_16BIT);

  ble_ad_set_service_uuids(ad, service_uuids, num_uuids);

  char device_name[BT_DEVICE_NAME_BUFFER_SIZE];
  bt_local_id_copy_device_name(device_name, true);
  ble_ad_set_local_name(ad, device_name);
  ble_ad_set_tx_power_level(ad);

  // Scan response part:
  ble_ad_start_scan_response(ad);

  // Add serial number in a Manufacturer Specific AD Type:
  struct PACKED ManufacturerSpecificData {
    uint8_t payload_type;
    char serial_number[MFG_SERIAL_NUMBER_SIZE];
    uint8_t hw_platform;
    uint8_t color;
    struct {
      uint8_t major;
      uint8_t minor;
      uint8_t patch;
    } fw_version;
    union {
      uint8_t flags;
      struct {
        bool is_running_recovery_firmware:1;
        bool is_first_use:1;
      };
    };
  } mfg_data = {
    .payload_type = 0 /* For future proofing. Only one type for now.*/,
    .hw_platform = TINTIN_METADATA.hw_platform,
    .color = mfg_info_get_watch_color(),
    .fw_version = {
      .major = GIT_MAJOR_VERSION,
      .minor = GIT_MINOR_VERSION,
      .patch = GIT_PATCH_VERSION,
    },
    .is_running_recovery_firmware = TINTIN_METADATA.is_recovery_firmware,
    .is_first_use = false, // !getting_started_is_complete(), // TODO
  };
  memcpy(&mfg_data.serial_number,
         mfg_get_serial_number(),
         MFG_SERIAL_NUMBER_SIZE);

  ble_ad_set_manufacturer_specific_data(ad,
                                       BT_VENDOR_ID,
                                       (const uint8_t *) &mfg_data,
                                       sizeof(struct ManufacturerSpecificData));

  // Values chosen according to:
  // "Accessory Design Guidelies for Apple Devices" 55.4 Advertising Data
  const GAPLEAdvertisingJobTerm advert_terms[] = {
      {
          // Extend this term from recommended 30s to 5min so user has e.g. time
          // to download or open mobile app.
          .duration_secs = 5 * 60,
          .min_interval_slots = 32, // 20ms
          .max_interval_slots = 32, // 20ms
      },
      {
          .duration_secs = GAPLE_ADVERTISING_DURATION_INFINITE,
          .min_interval_slots = 1636, // 1022.5ms
          .max_interval_slots = 1636, // 1022.5ms
      },
  };

  s_discovery_advert_job = gap_le_advert_schedule(
      ad, advert_terms, sizeof(advert_terms) / sizeof(GAPLEAdvertisingJobTerm),
      prv_job_unschedule_callback, NULL, GAPLEAdvertisingJobTagDiscovery);

  ble_ad_destroy(ad);
}

// -----------------------------------------------------------------------------
bool gap_le_slave_is_discoverable(void) {
  bool is_discoverable = false;
  bt_lock();
  {
    is_discoverable = (s_discovery_advert_job != NULL);
  }
  bt_unlock();
  return is_discoverable;
}

// -----------------------------------------------------------------------------
void gap_le_slave_set_discoverable(bool discoverable) {
  bt_lock();
  {
    // Always stop and re-start, so we start with the high rate again:
    gap_le_advert_unschedule(s_discovery_advert_job);
    if (discoverable) {
      prv_schedule_ad_job();
    }
  }
  bt_unlock();
}

// -----------------------------------------------------------------------------
void gap_le_slave_discovery_init(void) {
  bt_lock();
  {
    PBL_ASSERTN(!s_discovery_advert_job);
  }
  bt_unlock();
}

// -----------------------------------------------------------------------------
void gap_le_slave_discovery_deinit(void) {
  bt_lock();
  {
    gap_le_advert_unschedule(s_discovery_advert_job);
  }
  bt_unlock();
}
