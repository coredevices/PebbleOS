/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>
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
#include <services/bas/ble_svc_bas.h>
#include <services/dis/ble_svc_dis.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define NAME "PebbleOS HR"

#define HRS_UUID                  0x180DU
#define HRS_MEASUREMENT_UUID      0x2A37U
#define HRS_BODY_SENSOR_LOC_UUID  0x2A38U
#define HRS_BODY_SENSOR_LOC_WRIST 0x02U
#define BAS_UUID                  0x180FU
#define DIS_UUID                  0x180AU
#define APPEARANCE_HR_SENSOR      0x0340U

static PBL_SEM_DEFINE(s_synced, 0, 1);
static uint8_t s_own_addr_type;

static uint16_t s_measurement_handle;
static atomic_uint s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static atomic_bool s_subscribed;

/* Only notified: there is nothing to read. */
static int prv_measurement_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static int prv_body_sensor_loc_access(uint16_t conn_handle, uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt, void *arg) {
  const uint8_t loc = HRS_BODY_SENSOR_LOC_WRIST;

  if (os_mbuf_append(ctxt->om, &loc, sizeof(loc)) != 0) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  return 0;
}

static const struct ble_gatt_svc_def s_services[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = BLE_UUID16_DECLARE(HRS_UUID),
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = BLE_UUID16_DECLARE(HRS_MEASUREMENT_UUID),
            .access_cb = prv_measurement_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &s_measurement_handle,
          },
          {
            .uuid = BLE_UUID16_DECLARE(HRS_BODY_SENSOR_LOC_UUID),
            .access_cb = prv_body_sensor_loc_access,
            .flags = BLE_GATT_CHR_F_READ,
          },
          {},
        },
  },
  {},
};

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_advertise(void) {
  static const ble_uuid16_t uuids[] = {
    BLE_UUID16_INIT(HRS_UUID),
    BLE_UUID16_INIT(BAS_UUID),
    BLE_UUID16_INIT(DIS_UUID),
  };
  const char *name = ble_svc_gap_device_name();
  struct ble_hs_adv_fields fields = {
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    .name = (const uint8_t *)name,
    .name_len = strlen(name),
    .name_is_complete = 1,
    .uuids16 = uuids,
    .num_uuids16 = ARRAY_SIZE(uuids),
    .uuids16_is_complete = 1,
    .appearance = APPEARANCE_HR_SENSOR,
    .appearance_is_present = 1,
  };
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };
  int rc;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    printf("peripheral_hr: setting the data failed (%d)\n", rc);
    return;
  }

  rc = ble_gap_adv_start(s_own_addr_type, nullptr, BLE_HS_FOREVER, &params, prv_gap_event, nullptr);
  if (rc != 0) {
    printf("peripheral_hr: advertising failed (%d)\n", rc);
    return;
  }

  printf("peripheral_hr: advertising as %s\n", name);
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        printf("peripheral_hr: connection failed (%d)\n", event->connect.status);
        prv_advertise();
        break;
      }
      atomic_store(&s_conn_handle, event->connect.conn_handle);
      printf("peripheral_hr: connected\n");
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      atomic_store(&s_subscribed, false);
      atomic_store(&s_conn_handle, BLE_HS_CONN_HANDLE_NONE);
      printf("peripheral_hr: disconnected, reason 0x%03x\n", event->disconnect.reason);
      prv_advertise();
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      if (event->subscribe.attr_handle == s_measurement_handle) {
        atomic_store(&s_subscribed, event->subscribe.cur_notify != 0U);
        printf("peripheral_hr: measurements %s\n",
               event->subscribe.cur_notify != 0U ? "on" : "off");
      }
      break;
    default:
      break;
  }

  return 0;
}

static void prv_notify_measurement(uint8_t bpm) {
  /* Flags: an 8-bit value, no sensor contact, energy or RR-interval fields. */
  const uint8_t value[] = {0x00U, bpm};
  struct os_mbuf *om;
  int rc;

  om = ble_hs_mbuf_from_flat(value, sizeof(value));
  if (om == nullptr) {
    return;
  }

  /* Takes the buffer, even when it fails. */
  rc = ble_gatts_notify_custom(atomic_load(&s_conn_handle), s_measurement_handle, om);
  if (rc != 0) {
    printf("peripheral_hr: notifying failed (%d)\n", rc);
  }
}

static void prv_on_sync(void) {
  pbl_sem_give(&s_synced);
}

static void prv_on_reset(int reason) {
  printf("peripheral_hr: host reset, reason %d\n", reason);
}

int main(void) {
  uint8_t addr[6];
  uint8_t battery = 100U;
  int rc;

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;

  /* NimBLE's own services, then the one of this sample. */
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_bas_init();
  ble_svc_dis_init();

  rc = ble_gatts_count_cfg(s_services);
  if (rc == 0) {
    rc = ble_gatts_add_svcs(s_services);
  }
  if (rc == 0) {
    rc = ble_svc_gap_device_name_set(NAME);
  }
  if (rc == 0) {
    rc = ble_svc_dis_manufacturer_name_set("PebbleOS");
  }
  if (rc == 0) {
    rc = ble_svc_dis_model_number_set("Heart rate sample");
  }
  if (rc != 0) {
    printf("peripheral_hr: adding the services failed (%d)\n", rc);
    return rc;
  }

  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  }
  if (rc == 0) {
    rc = ble_hs_id_copy_addr(s_own_addr_type, addr, nullptr);
  }
  if (rc != 0) {
    printf("peripheral_hr: no address (%d)\n", rc);
    return rc;
  }

  printf("peripheral_hr: address %02x:%02x:%02x:%02x:%02x:%02x (%s)\n", addr[5], addr[4], addr[3],
         addr[2], addr[1], addr[0], s_own_addr_type == BLE_OWN_ADDR_PUBLIC ? "public" : "random");

  (void)ble_svc_bas_battery_level_set(battery);
  prv_advertise();

  /* A simulated rate every second, and a battery that drains every ten. */
  for (unsigned int i = 1U;; i++) {
    pbl_thread_sleep(PBL_SEC(1));

    if (atomic_load(&s_subscribed)) {
      prv_notify_measurement(60U + (i % 20U));
    }

    if (i % 10U == 0U && battery > 0U) {
      battery--;
      /* Notifies a subscribed peer itself. */
      (void)ble_svc_bas_battery_level_set(battery);
      printf("peripheral_hr: battery at %u%%\n", battery);
    }
  }

  return 0;
}
