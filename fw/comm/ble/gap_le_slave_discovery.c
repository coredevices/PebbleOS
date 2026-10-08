/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gap_le_slave_discovery.h"

#include "gap_le_advert.h"

#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/bluetooth/types.h>
#include <pbl/btutil/bt_uuid.h>
#include <pbl/kernel/compiler.h>
#include <pbl/services/bluetooth/ble_hrm.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/util/size.h>
#include <pbl/version.h>

#include <applib/bluetooth/ble_ad_parse.h>
#include <board/board.h>
#include <comm/bt_lock.h>
#include <mfg/mfg_info.h>
#include <mfg/mfg_serials.h>
#include <system/passert.h>
#include <system/version.h>

static GAPLEAdvertisingJobRef s_discovery_advert_job;

// -----------------------------------------------------------------------------
//! Handles unscheduling of the discovery advertisement job.
static void prv_job_unschedule_callback(GAPLEAdvertisingJobRef job, bool completed, void *cb_data) {
  // Cleanup:
  s_discovery_advert_job = NULL;
}

// -----------------------------------------------------------------------------
//! Schedules the discovery advertisement job.
//! We don't want to be advertising at a high rate infinitely. When duration
//! is 0, a short period of high-rate advertising will be used. When this short
//! period is completed, an indefinite, low-rate job will be scheduled.
static void prv_schedule_ad_job(bool fast) {
  struct pbl_bt_ad_data *ad = ble_ad_create();

  // Advertisement part:
  // Centrals will be filtering on Service UUID first. Assuming that the
  // central is only doing a scan request if the Service UUID matches with their
  // interests, to save radio time / battery life we keep the advertisement part
  // as "small" as possible (21 bytes currently).
  // Advertise "BR/EDR Not Supported" alongside General Discoverable: these are
  // BLE-only watches, so dual-mode hosts must connect over LE instead of attempting
  // a classic page (which would time out).
  ble_ad_set_flags(
      ad, PBL_BT_AD_FLAGS_GEN_DISCOVERABLE_MASK | PBL_BT_AD_FLAGS_BR_EDR_NOT_SUPPORTED_MASK);

  // *DO NOT* use pbl_bt_pebble_uuid_expand() here!
  // ble_ad_set_service_uuids() will be "smart" and include only the 16-bit UUID, but only if the
  // BT SIG Base UUID is used.
  Uuid service_uuids[2];
  size_t num_uuids = 0;

#ifdef CONFIG_SERVICE_BLE_HRM
  // NOTE: The HRM service has to be first in the list because otherwise the Pebble won't
  // show up as an HRM device in Strava for Android...
  if (ble_hrm_is_supported_and_enabled()) {
    service_uuids[num_uuids++] = bt_uuid_expand_16bit(0x180D); // Heart Rate Service
  }
#endif

  // Pebble Pairing Service UUID:
  service_uuids[num_uuids++] = bt_uuid_expand_16bit(PBL_BT_PPS_UUID_16BIT);

  ble_ad_set_service_uuids(ad, service_uuids, num_uuids);

  char device_name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  bt_local_id_copy_device_name(device_name, true);
  ble_ad_set_local_name(ad, device_name);
  ble_ad_set_tx_power_level(ad);

  // Scan response part:
  ble_ad_start_scan_response(ad);

  // Add serial number in a Manufacturer Specific AD Type:
  struct PBL_PACKED ManufacturerSpecificData {
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
        bool is_running_recovery_firmware : 1;
        bool is_first_use : 1;
      };
    };
  } mfg_data = {
    .payload_type = 0 /* For future proofing. Only one type for now.*/,
    .hw_platform = TINTIN_METADATA.hw_platform,
    .color = mfg_info_get_watch_color(),
    .fw_version =
        {
          .major = PBL_VERSION_MAJOR,
          .minor = PBL_VERSION_MINOR,
          .patch = PBL_VERSION_PATCH,
        },
    .is_running_recovery_firmware = TINTIN_METADATA.is_recovery_firmware,
    .is_first_use = false, // !getting_started_is_complete(), // TODO
  };
  memcpy(&mfg_data.serial_number, mfg_get_serial_number(), MFG_SERIAL_NUMBER_SIZE);

  ble_ad_set_manufacturer_specific_data(ad, BT_VENDOR_ID, (const uint8_t *)&mfg_data,
                                        sizeof(struct ManufacturerSpecificData));

  // Values chosen according to Apple Accessory Design Guidelines.
  const GAPLEAdvertisingJobTerm advert_terms[] = {
    {
      // Extend this term from recommended 30s to 5min so user has e.g. time
      // to download or open mobile app.
      .duration_secs = 5 * 60,
      .interval = GAPLEAdvertisingInterval_Short,
    },
    {
      .duration_secs = GAPLE_ADVERTISING_DURATION_INFINITE,
      .interval = GAPLEAdvertisingInterval_Long,
    },
  };

  const size_t first_term = fast ? 0 : 1;
  s_discovery_advert_job =
      gap_le_advert_schedule(ad, &advert_terms[first_term], ARRAY_LENGTH(advert_terms) - first_term,
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
      prv_schedule_ad_job(true);
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

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_adv_slow(const struct pbl_shell *sh, size_t argc, char **argv) {
  int rv = 0;

  bt_lock();
  {
    if (s_discovery_advert_job) {
      gap_le_advert_unschedule(s_discovery_advert_job);
      prv_schedule_ad_job(false);
    } else {
      pbl_shell_error(sh, "not discoverable");
      rv = -EINVAL;
    }
  }
  bt_unlock();
  return rv;
}

PBL_SHELL_SUBCMD_ADD(sub_bt, adv_slow, NULL, "Advertise for discovery at the slow rate",
                     prv_cmd_adv_slow, 0, 0);
#endif
