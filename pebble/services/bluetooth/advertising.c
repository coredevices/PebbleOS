/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/bluetooth/gap.h>
#include <pbl/bluetooth/init.h>
#include <pbl/bluetooth/pebble_bt.h>
#include <pbl/bluetooth/types.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
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

/* Tried again after this long when the stack refuses to advertise. */
#define RETRY_S 1

/*
 * A phone that reconnects and drops over and over (e.g. supervision timeouts in bad RF
 * conditions) would keep the watch at the short interval. Past the threshold within a window,
 * reconnection goes to the long interval right away.
 */
#define CHURN_WINDOW_S  60
#define CHURN_THRESHOLD 3

/* In order of priority. */
enum mode {
  MODE_DISCOVERY,
  MODE_RECONNECT,
  MODE_NUM,
};

struct mode_state {
  bool enabled;
  /* Uptime until which the short interval is used. */
  uint64_t short_until;
};

static PBL_MUTEX_DEFINE(s_lock);
static struct mode_state s_modes[MODE_NUM];
static struct pbl_delayed_work s_work;
static bool s_running;
static bool s_connected;
static bool s_airing;
static enum mode s_airing_mode;
static bool s_airing_short;
static uint64_t s_churn_window_start;
static uint32_t s_churn_count;

static uint64_t prv_secs_from_now(uint32_t secs) {
  return pbl_uptime_ticks() + (uint64_t)secs * PBL_TICK_HZ;
}

static void prv_analytics_stop(void) {
  PBL_ANALYTICS_TIMER_STOP(ble_adv_short_intvl_time_ms);
  PBL_ANALYTICS_TIMER_STOP(ble_adv_long_intvl_time_ms);
}

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
    .company_id = CONFIG_BT_VENDOR_ID,
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
  bt_local_id_copy_device_name(name, true);
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

static int prv_set_reconnect_fields(void) {
  int rc;

  /* A bonded phone reconnects to the address alone: an empty payload saves power. */
  rc = ble_gap_adv_set_data(nullptr, 0);
  if (rc != 0) {
    return rc;
  }

  return ble_gap_adv_rsp_set_data(nullptr, 0);
}

static int prv_start(enum mode mode, bool short_interval) {
  const uint32_t interval_ms = short_interval ? SHORT_INTERVAL_MS : LONG_INTERVAL_MS;
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
    .itvl_min = BLE_GAP_ADV_ITVL_MS(interval_ms),
    .itvl_max = BLE_GAP_ADV_ITVL_MS(interval_ms),
  };
  uint8_t own_addr_type;
  int rc;

  switch (mode) {
    case MODE_DISCOVERY:
      rc = prv_set_discovery_fields();
      break;
    default:
      rc = prv_set_reconnect_fields();
      break;
  }
  if (rc != 0) {
    return rc;
  }

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    return rc;
  }

  return ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &params,
                           pbl_bt_gap_event_handler, nullptr);
}

static void prv_stop(void) {
  if (ble_gap_adv_active()) {
    int rc = ble_gap_adv_stop();
    if (rc != 0) {
      PBL_LOG_ERR("Could not stop advertising (%d)", rc);
    }
  }

  if (s_airing) {
    prv_analytics_stop();
    s_airing = false;
  }
}

/* Airs what the modes ask for now, and comes back when that next changes. */
static void prv_update(void) {
  const uint64_t now = pbl_uptime_ticks();
  uint64_t next = UINT64_MAX;
  enum mode mode = MODE_NUM;
  bool short_interval;
  int rc;

  for (int i = MODE_NUM - 1; i >= 0; i--) {
    if (s_modes[i].enabled) {
      mode = i;
    }
  }

  if (!s_running || s_connected || mode == MODE_NUM) {
    pbl_delayed_work_cancel(&s_work);
    prv_stop();
    return;
  }

  short_interval = now < s_modes[mode].short_until;
  if (short_interval) {
    next = s_modes[mode].short_until;
  }
  if (next != UINT64_MAX) {
    pbl_delayed_work_reschedule(&pbl_sys_workq, &s_work, PBL_TICKS(next - now));
  } else {
    pbl_delayed_work_cancel(&s_work);
  }

  if (s_airing && mode == s_airing_mode && short_interval == s_airing_short) {
    return;
  }

  prv_stop();
  rc = prv_start(mode, short_interval);
  if (rc != 0) {
    PBL_LOG_ERR("Could not advertise (%d)", rc);
    pbl_delayed_work_reschedule(&pbl_sys_workq, &s_work,
                                PBL_TICKS(PBL_MIN(next - now, (uint64_t)RETRY_S * PBL_TICK_HZ)));
    return;
  }

  PBL_LOG_DBG("Advertising: mode %d, %s interval", mode, short_interval ? "short" : "long");
  s_airing = true;
  s_airing_mode = mode;
  s_airing_short = short_interval;
  if (short_interval) {
    PBL_ANALYTICS_TIMER_START(ble_adv_short_intvl_time_ms);
  } else {
    PBL_ANALYTICS_TIMER_START(ble_adv_long_intvl_time_ms);
  }
}

static void prv_work_fn(struct pbl_work *work) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

static bool prv_is_churning(void) {
  const uint64_t now = pbl_uptime_ticks();

  if (now - s_churn_window_start > (uint64_t)CHURN_WINDOW_S * PBL_TICK_HZ) {
    s_churn_window_start = now;
    s_churn_count = 0;
  }

  return ++s_churn_count > CHURN_THRESHOLD;
}

static void prv_restart_reconnect_interval(void) {
  if (prv_is_churning()) {
    PBL_LOG_WRN("Reconnect churn: advertising at long interval only");
    s_modes[MODE_RECONNECT].short_until = 0;
  } else {
    s_modes[MODE_RECONNECT].short_until = prv_secs_from_now(RECONNECT_SHORT_S);
  }
}

void bt_advertising_init(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  pbl_delayed_work_init(&s_work, prv_work_fn);
  memset(s_modes, 0, sizeof(s_modes));
  s_connected = false;
  s_airing = false;
  s_running = true;
  pbl_mutex_unlock(&s_lock);
}

void bt_advertising_deinit(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_running = false;
  memset(s_modes, 0, sizeof(s_modes));
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

void bt_advertising_set_discoverable(bool discoverable) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_modes[MODE_DISCOVERY].enabled = discoverable;
  s_modes[MODE_DISCOVERY].short_until = prv_secs_from_now(DISCOVERY_SHORT_S);
  /* Air again in any case: the payload may have changed. */
  if (s_airing && s_airing_mode == MODE_DISCOVERY) {
    prv_stop();
  }
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

void bt_advertising_set_reconnectable(bool reconnectable) {
#ifdef CONFIG_RECOVERY_FW
  return;
#endif
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_modes[MODE_RECONNECT].enabled != reconnectable) {
    s_modes[MODE_RECONNECT].enabled = reconnectable;
    if (reconnectable) {
      prv_restart_reconnect_interval();
    }
    prv_update();
  }
  pbl_mutex_unlock(&s_lock);
}

void bt_advertising_handle_connect(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_connected = true;
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

void bt_advertising_handle_disconnect(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  s_connected = false;
  if (s_modes[MODE_RECONNECT].enabled) {
    prv_restart_reconnect_interval();
  }
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_handle_host_resynced(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!s_running) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  if (s_airing) {
    prv_analytics_stop();
    s_airing = false;
  }
  prv_update();
  pbl_mutex_unlock(&s_lock);
}
