/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>

#define SCAN_MS     5000
#define MAX_DEVICES 32
#define NAME_LEN    32

static PBL_SEM_DEFINE(s_synced, 0, 1);
static PBL_SEM_DEFINE(s_done, 0, 1);

static ble_addr_t s_devices[MAX_DEVICES];
static size_t s_num_devices;

static void prv_on_sync(void) {
  pbl_sem_give(&s_synced);
}

static void prv_on_reset(int reason) {
  printf("observer: host reset, reason %d\n", reason);
}

static bool prv_seen(const ble_addr_t *addr) {
  for (size_t i = 0U; i < s_num_devices; i++) {
    if (ble_addr_cmp(&s_devices[i], addr) == 0) {
      return true;
    }
  }

  return false;
}

static void prv_report(const struct ble_gap_disc_desc *desc) {
  struct ble_hs_adv_fields fields;
  char name[NAME_LEN] = "";
  const uint8_t *a = desc->addr.val;

  if (ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data) == 0 &&
      fields.name != nullptr) {
    size_t len = fields.name_len < sizeof(name) - 1U ? fields.name_len : sizeof(name) - 1U;
    memcpy(name, fields.name, len);
    name[len] = '\0';
  }

  printf("observer: %02x:%02x:%02x:%02x:%02x:%02x (%s), %d dBm%s%s\n", a[5], a[4], a[3], a[2], a[1],
         a[0], desc->addr.type == BLE_ADDR_PUBLIC ? "public" : "random", desc->rssi,
         name[0] != '\0' ? ", " : "", name);
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
      if (s_num_devices < MAX_DEVICES && !prv_seen(&event->disc.addr)) {
        s_devices[s_num_devices++] = event->disc.addr;
        prv_report(&event->disc);
      }
      break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
      pbl_sem_give(&s_done);
      break;
    default:
      break;
  }

  return 0;
}

int main(void) {
  uint8_t own_addr_type;
  int rc;

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;
  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
  }
  if (rc != 0) {
    printf("observer: no address (%d)\n", rc);
    return rc;
  }

  /* Passive: listen to advertisements without asking for scan responses. The controller drops
   * the duplicates, as far as its own list of them goes.
   */
  const struct ble_gap_disc_params params = {
    .passive = 1,
    .filter_duplicates = 1,
  };

  rc = ble_gap_disc(own_addr_type, SCAN_MS, &params, prv_gap_event, nullptr);
  if (rc != 0) {
    printf("observer: scanning failed (%d)\n", rc);
    return rc;
  }

  printf("observer: scanning for %d seconds\n", SCAN_MS / 1000);

  pbl_sem_take(&s_done, PBL_FOREVER);

  printf("observer: %u device%s found\n", (unsigned int)s_num_devices,
         s_num_devices == 1U ? "" : "s");

  printf("Sample done\n");

  return 0;
}
