/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/adv.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/bluetooth/pebble_pairing_service.h>
#include <pbl/kernel/compiler.h>
#include <pbl/logging/logging.h>
#include <pbl/services/bluetooth/advertising.h>
#include <pbl/services/bluetooth/local_id.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/version.h>

#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <mfg/mfg_info.h>
#include <mfg/mfg_serials.h>
#include <system/version.h>

PBL_LOG_MODULE_DECLARE(service_bluetooth, CONFIG_SERVICE_BLUETOOTH_LOG_LEVEL);

/* As the Accessory Design Guidelines for Apple Devices recommend. */
#define SHORT_INTERVAL_MS 20
#define LONG_INTERVAL_MS  1022

/* Longer than the 30 s recommended: time to install or open the phone app. */
#define DISCOVERY_SHORT_S (5 * 60)
#define RECONNECT_SHORT_S 30

/*
 * A phone that reconnects and drops over and over (e.g. supervision timeouts in bad RF
 * conditions) would keep the watch at the short interval. Past the threshold within a window,
 * reconnection goes to the long interval right away.
 */
#define CHURN_WINDOW_S  60
#define CHURN_THRESHOLD 3

static int prv_set_discovery_fields(void) {
  struct PBL_PACKED {
    uint16_t company_id;
    uint8_t payload_type;
    char serial_number[MFG_SERIAL_NUMBER_SIZE];
    uint8_t hw_platform;
    uint8_t color;
    uint8_t fw_version[3];
    uint8_t flags;
  } mfg_data = {
    .company_id = CONFIG_SERVICE_BLUETOOTH_VENDOR_ID,
    .hw_platform = TINTIN_METADATA.hw_platform,
    .color = mfg_info_get_watch_color(),
    .fw_version = {PBL_VERSION_MAJOR, PBL_VERSION_MINOR, PBL_VERSION_PATCH},
    .flags = TINTIN_METADATA.is_recovery_firmware ? 0x01 : 0x00,
  };
  char name[PBL_BT_DEVICE_NAME_BUFFER_SIZE];
  static const ble_uuid16_t uuid = BLE_UUID16_INIT(PBL_BT_PPS_UUID_16BIT);
  struct ble_hs_adv_fields fields = {
    /* No BR/EDR, or dual-mode hosts try a classic page, which times out. */
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    .uuids16 = &uuid,
    .num_uuids16 = 1,
    .uuids16_is_complete = 1,
    .name = (const uint8_t *)name,
    .tx_pwr_lvl_is_present = 1,
    .tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO,
  };
  const struct ble_hs_adv_fields rsp_fields = {
    .mfg_data = (const uint8_t *)&mfg_data,
    .mfg_data_len = sizeof(mfg_data),
  };
  size_t name_max;
  int rc;

  /* What flags, services and TX power leave, less the name's own header. */
  name_max = BLE_HS_ADV_MAX_SZ - 3 - (2 + 2 * fields.num_uuids16) - 3 - 2;
  bt_local_id_copy_device_name(name);
  name[sizeof(name) - 1] = '\0';
  fields.name_len = PBL_MIN(strlen(name), name_max);
  fields.name_is_complete = (name[fields.name_len] == '\0');

  memcpy(mfg_data.serial_number, mfg_get_serial_number(), MFG_SERIAL_NUMBER_SIZE);

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    return rc;
  }

  return ble_gap_adv_rsp_set_fields(&rsp_fields);
}

#ifdef CONFIG_SERVICE_BLUETOOTH_ADV_RECONNECT
static int prv_set_reconnect_fields(void) {
  int rc;

  /* A bonded phone reconnects to the address alone: an empty payload saves power. */
  rc = ble_gap_adv_set_data(nullptr, 0);
  if (rc != 0) {
    return rc;
  }

  return ble_gap_adv_rsp_set_data(nullptr, 0);
}

static struct pbl_bt_adv s_reconnect = {
  .set_data = prv_set_reconnect_fields,
  .resume_short_secs = RECONNECT_SHORT_S,
};
#endif

static void prv_changed(const struct pbl_bt_adv *adv, bool short_interval) {
  PBL_ANALYTICS_TIMER_STOP(ble_adv_short_intvl_time_ms);
  PBL_ANALYTICS_TIMER_STOP(ble_adv_long_intvl_time_ms);

  if (adv == nullptr) {
    return;
  }

  if (short_interval) {
    PBL_ANALYTICS_TIMER_START(ble_adv_short_intvl_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(ble_adv_long_intvl_time_ms);
  }
}

static struct pbl_bt_adv s_discovery = {
  .set_data = prv_set_discovery_fields,
};

/* In order of priority. */
static struct pbl_bt_adv *const s_advs[] = {
  &s_discovery,
#ifdef CONFIG_SERVICE_BLUETOOTH_ADV_RECONNECT
  &s_reconnect,
#endif
};

static const struct pbl_bt_adv_config s_config = {
  .advs = s_advs,
  .num_advs = PBL_ARRAY_SIZE(s_advs),
  .short_interval_ms = SHORT_INTERVAL_MS,
  .long_interval_ms = LONG_INTERVAL_MS,
  .churn_threshold = CHURN_THRESHOLD,
  .churn_window_secs = CHURN_WINDOW_S,
  .gap_event = pbl_bt_gap_event_handler,
  .changed = prv_changed,
};

void bt_advertising_init(void) {
  pbl_bt_adv_start(&s_config);
}

void bt_advertising_deinit(void) {
  pbl_bt_adv_stop();
}

void bt_advertising_set_discoverable(bool discoverable) {
  if (discoverable) {
    pbl_bt_adv_enable(&s_discovery, DISCOVERY_SHORT_S);
  } else {
    pbl_bt_adv_disable(&s_discovery);
  }
}

void bt_advertising_set_reconnectable(bool reconnectable) {
#ifdef CONFIG_SERVICE_BLUETOOTH_ADV_RECONNECT
  if (reconnectable) {
    pbl_bt_adv_enable(&s_reconnect, RECONNECT_SHORT_S);
  } else {
    pbl_bt_adv_disable(&s_reconnect);
  }
#endif
}
