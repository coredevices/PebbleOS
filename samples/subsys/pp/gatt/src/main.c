/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sample.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/util/size.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define NAME "PebbleOS"

static PBL_SEM_DEFINE(s_synced, 0, 1);
static uint8_t s_own_addr_type;

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_advertise(void) {
  const struct ble_hs_adv_fields fields = {
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
  };
  const struct ble_hs_adv_fields rsp_fields = {
    .name = (const uint8_t *)NAME,
    .name_len = strlen(NAME),
    .name_is_complete = 1,
  };
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };
  int rc;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc == 0) {
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  }
  if (rc == 0) {
    rc = ble_gap_adv_start(s_own_addr_type, nullptr, BLE_HS_FOREVER, &params, prv_gap_event,
                           nullptr);
  }
  if (rc != 0) {
    printf("pp_gatt: advertising failed (%d)\n", rc);
    return;
  }

  printf("pp_gatt: advertising as %s\n", NAME);
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_gap_conn_desc desc;

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        prv_advertise();
        break;
      }
      printf("pp_gatt: connected\n");
      /* The session is only opened over an encrypted link. */
      (void)ble_gap_security_initiate(event->connect.conn_handle);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      printf("pp_gatt: disconnected, reason 0x%03x\n", event->disconnect.reason);
      sample_pp_stop();
      prv_advertise();
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if (event->enc_change.status != 0) {
        printf("pp_gatt: encryption failed (%d)\n", event->enc_change.status);
        break;
      }
      sample_pp_start(event->enc_change.conn_handle);
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      /* A bonded peer that lost its keys: forget ours, and pair again. */
      if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
        (void)ble_store_util_delete_peer(&desc.peer_id_addr);
      }
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    default:
      break;
  }

  return 0;
}

static void prv_on_sync(void) {
  pbl_sem_give(&s_synced);
}

int main(void) {
  int rc;

  sample_pp_init();

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();

  ble_svc_gap_init();
  ble_svc_gatt_init();

  rc = ble_svc_gap_device_name_set(NAME);
  if (rc != 0) {
    printf("pp_gatt: setting the name failed (%d)\n", rc);
    return rc;
  }

  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  }
  if (rc != 0) {
    printf("pp_gatt: no address (%d)\n", rc);
    return rc;
  }

  prv_advertise();

  return 0;
}
