/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "adv.h"

#include <pbl/bluetooth/adv.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>

#include <host/ble_hs.h>

PBL_LOG_MODULE_DEFINE(bt_adv, CONFIG_BT_ADV_LOG_LEVEL);

/* Tried again after this long when the stack refuses to advertise. */
#define RETRY_S 1

static void prv_work_fn(struct pbl_work *work);

static PBL_MUTEX_DEFINE(s_lock);
static PBL_DELAYED_WORK_DEFINE(s_work, prv_work_fn);
static struct ble_gap_event_listener s_listener;
static const struct pbl_bt_adv_config *s_config;
static bool s_connected;
static const struct pbl_bt_adv *s_airing;
static bool s_airing_short;
static uint64_t s_churn_window_start;
static uint32_t s_churn_count;

static uint64_t prv_secs_from_now(uint32_t secs) {
  return pbl_uptime_ticks() + (uint64_t)secs * PBL_TICK_HZ;
}

static void prv_set_airing(const struct pbl_bt_adv *adv, bool short_interval) {
  if (adv == s_airing && (adv == nullptr || short_interval == s_airing_short)) {
    return;
  }

  s_airing = adv;
  s_airing_short = short_interval;
  if (s_config != nullptr && s_config->changed != nullptr) {
    s_config->changed(adv, short_interval);
  }
}

static int prv_air(const struct pbl_bt_adv *adv, bool short_interval) {
  const uint32_t interval_ms =
      short_interval ? s_config->short_interval_ms : s_config->long_interval_ms;
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
    .itvl_min = BLE_GAP_ADV_ITVL_MS(interval_ms),
    .itvl_max = BLE_GAP_ADV_ITVL_MS(interval_ms),
  };
  uint8_t own_addr_type;
  int rc;

  rc = adv->set_data();
  if (rc != 0) {
    return rc;
  }

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    return rc;
  }

  return ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &params, s_config->gap_event,
                           s_config->gap_event_arg);
}

static void prv_silence(void) {
  if (ble_gap_adv_active()) {
    int rc = ble_gap_adv_stop();

    if (rc != 0) {
      PBL_LOG_ERR("Could not stop advertising (%d)", rc);
    }
  }

  prv_set_airing(nullptr, false);
}

/* Airs what is enabled now, and comes back when that next changes. s_lock must be held. */
static void prv_update(void) {
  const uint64_t now = pbl_uptime_ticks();
  const struct pbl_bt_adv *adv = nullptr;
  uint64_t next = UINT64_MAX;
  bool short_interval;
  int rc;

  if (s_config != nullptr && !s_connected) {
    for (size_t i = 0; i < s_config->num_advs; i++) {
      if (s_config->advs[i]->enabled) {
        adv = s_config->advs[i];
        break;
      }
    }
  }

  if (adv == nullptr) {
    pbl_delayed_work_cancel(&s_work);
    prv_silence();
    return;
  }

  short_interval = now < adv->short_until;
  if (short_interval) {
    next = adv->short_until;
    pbl_delayed_work_reschedule(&pbl_sys_workq, &s_work, PBL_TICKS(next - now));
  } else {
    pbl_delayed_work_cancel(&s_work);
  }

  if (adv == s_airing && short_interval == s_airing_short) {
    return;
  }

  prv_silence();
  rc = prv_air(adv, short_interval);
  if (rc != 0) {
    PBL_LOG_ERR("Could not advertise (%d)", rc);
    pbl_delayed_work_reschedule(&pbl_sys_workq, &s_work,
                                PBL_TICKS(PBL_MIN(next - now, (uint64_t)RETRY_S * PBL_TICK_HZ)));
    return;
  }

  PBL_LOG_DBG("Advertising at the %s interval", short_interval ? "short" : "long");
  prv_set_airing(adv, short_interval);
}

static void prv_work_fn(struct pbl_work *work) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  prv_update();
  pbl_mutex_unlock(&s_lock);
}

static bool prv_is_churning(void) {
  const uint64_t now = pbl_uptime_ticks();

  if (s_config->churn_threshold == 0U) {
    return false;
  }

  if (now - s_churn_window_start > (uint64_t)s_config->churn_window_secs * PBL_TICK_HZ) {
    s_churn_window_start = now;
    s_churn_count = 0;
  }

  return ++s_churn_count > s_config->churn_threshold;
}

/* s_lock must be held. */
static void prv_resume(void) {
  const bool churning = prv_is_churning();

  if (churning) {
    PBL_LOG_WRN("Connections come and go: advertising at the long interval");
  }

  for (size_t i = 0; i < s_config->num_advs; i++) {
    struct pbl_bt_adv *adv = s_config->advs[i];

    if (adv->resume_short_secs != 0U) {
      adv->short_until = churning ? 0U : prv_secs_from_now(adv->resume_short_secs);
    }
  }
}

static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  if (event->type != BLE_GAP_EVENT_CONNECT && event->type != BLE_GAP_EVENT_DISCONNECT) {
    return 0;
  }

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_config != nullptr) {
    if (event->type == BLE_GAP_EVENT_CONNECT) {
      /* A connection that failed ended the advertising all the same. */
      s_connected = (event->connect.status == 0);
      prv_set_airing(nullptr, false);
    } else {
      s_connected = false;
      prv_resume();
    }
    prv_update();
  }
  pbl_mutex_unlock(&s_lock);

  return 0;
}

void pbl_bt_adv_start(const struct pbl_bt_adv_config *config) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  for (size_t i = 0; i < config->num_advs; i++) {
    config->advs[i]->enabled = false;
    config->advs[i]->short_until = 0U;
  }
  s_config = config;
  s_connected = false;
  s_airing = nullptr;
  s_churn_count = 0;
  pbl_mutex_unlock(&s_lock);

  /* Listeners outlive the stack: a second registration is refused. */
  (void)ble_gap_event_listener_register(&s_listener, prv_gap_event, nullptr);
}

void pbl_bt_adv_stop(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_config != nullptr) {
    for (size_t i = 0; i < s_config->num_advs; i++) {
      s_config->advs[i]->enabled = false;
    }
    prv_update();
    s_config = nullptr;
  }
  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_adv_enable(struct pbl_bt_adv *adv, uint16_t short_secs) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_config != nullptr) {
    adv->enabled = true;
    adv->short_until = prv_secs_from_now(short_secs);
    /* Air it again in any case: its payloads may have changed. */
    if (s_airing == adv) {
      prv_silence();
    }
    prv_update();
  }
  pbl_mutex_unlock(&s_lock);
}

void pbl_bt_adv_disable(struct pbl_bt_adv *adv) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_config != nullptr && adv->enabled) {
    adv->enabled = false;
    prv_update();
  }
  pbl_mutex_unlock(&s_lock);
}

void pbl_bt__adv_handle_sync(void) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_config != nullptr) {
    /* The controller forgot what it aired. */
    s_connected = false;
    prv_set_airing(nullptr, false);
    prv_update();
  }
  pbl_mutex_unlock(&s_lock);
}
