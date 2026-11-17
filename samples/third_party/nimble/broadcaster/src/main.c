/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/types.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>

/* Reserved for testing by the Bluetooth SIG. */
#define COMPANY_ID 0xFFFFU
#define NAME       "PebbleOS"
#define UPDATES    5

static PBL_SEM_DEFINE(s_synced, 0, 1);

static void prv_on_sync(void) {
  pbl_sem_give(&s_synced);
}

static void prv_on_reset(int reason) {
  printf("broadcaster: host reset, reason %d\n", reason);
}

static int prv_set_adv_data(uint8_t counter) {
  const uint8_t mfg_data[] = {COMPANY_ID & 0xFFU, COMPANY_ID >> 8, counter};
  struct ble_hs_adv_fields fields = {
    .flags = BLE_HS_ADV_F_BREDR_UNSUP,
    .name = (const uint8_t *)NAME,
    .name_len = strlen(NAME),
    .name_is_complete = 1,
    .mfg_data = mfg_data,
    .mfg_data_len = sizeof(mfg_data),
  };

  return ble_gap_adv_set_fields(&fields);
}

int main(void) {
  uint8_t own_addr_type;
  uint8_t addr[6];
  int rc;

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;
  nimble_port_pebble_init();

  /* The host starts on its own thread, and is ready once in sync with the controller. */
  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
  }
  if (rc == 0) {
    rc = ble_hs_id_copy_addr(own_addr_type, addr, nullptr);
  }
  if (rc != 0) {
    printf("broadcaster: no address (%d)\n", rc);
    return rc;
  }

  printf("broadcaster: address %02x:%02x:%02x:%02x:%02x:%02x (%s)\n", addr[5], addr[4], addr[3],
         addr[2], addr[1], addr[0], own_addr_type == BLE_OWN_ADDR_PUBLIC ? "public" : "random");

  rc = prv_set_adv_data(0U);
  if (rc != 0) {
    printf("broadcaster: setting the data failed (%d)\n", rc);
    return rc;
  }

  /* Neither connectable nor discoverable: a scanner only gets to see the data. */
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_NON,
    .disc_mode = BLE_GAP_DISC_MODE_NON,
    .itvl_min = BLE_GAP_ADV_ITVL_MS(100),
    .itvl_max = BLE_GAP_ADV_ITVL_MS(150),
  };

  rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &params, nullptr, nullptr);
  if (rc != 0) {
    printf("broadcaster: advertising failed (%d)\n", rc);
    return rc;
  }

  printf("broadcaster: advertising\n");

  /* The data can change while advertising. */
  for (uint8_t i = 1U; i <= UPDATES; i++) {
    pbl_thread_sleep(PBL_SEC(1));

    rc = prv_set_adv_data(i);
    if (rc != 0) {
      printf("broadcaster: updating the data failed (%d)\n", rc);
      return rc;
    }

    printf("broadcaster: counter %u\n", i);
  }

  (void)ble_gap_adv_stop();

  printf("Sample done\n");

  return 0;
}
