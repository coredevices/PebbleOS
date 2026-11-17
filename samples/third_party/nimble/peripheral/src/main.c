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
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define NAME        "PebbleOS"
#define MESSAGE_MAX 20U

/* 5e1a0000-7c3b-4f0e-9d2a-6b8c1f4e0a37, and its characteristics after it. */
#define SAMPLE_UUID(n)                                                                        \
  BLE_UUID128_INIT(0x37, 0x0a, 0x4e, 0x1f, 0x8c, 0x6b, 0x2a, 0x9d, 0x0e, 0x4f, 0x3b, 0x7c, n, \
                   0x00, 0x1a, 0x5e)

static const ble_uuid128_t s_svc_uuid = SAMPLE_UUID(0x00);
static const ble_uuid128_t s_message_uuid = SAMPLE_UUID(0x01);
static const ble_uuid128_t s_counter_uuid = SAMPLE_UUID(0x02);

static PBL_SEM_DEFINE(s_synced, 0, 1);
static uint8_t s_own_addr_type;

/* Only the host thread touches the message. */
static char s_message[MESSAGE_MAX + 1U] = "Hello";
static uint16_t s_counter_handle;
static atomic_uint s_counter;
static atomic_bool s_subscribed;

static int prv_message_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
  uint16_t len;

  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
      if (os_mbuf_append(ctxt->om, s_message, strlen(s_message)) != 0) {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      return 0;
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
      if (OS_MBUF_PKTLEN(ctxt->om) > MESSAGE_MAX) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      }
      if (ble_hs_mbuf_to_flat(ctxt->om, s_message, MESSAGE_MAX, &len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
      }
      s_message[len] = '\0';
      printf("peripheral: message is now \"%s\"\n", s_message);
      return 0;
    default:
      return BLE_ATT_ERR_UNLIKELY;
  }
}

/* Also called when notifying, for the value to send. */
static int prv_counter_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
  uint8_t value[4];

  put_le32(value, atomic_load(&s_counter));
  if (os_mbuf_append(ctxt->om, value, sizeof(value)) != 0) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  return 0;
}

static const struct ble_gatt_svc_def s_services[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &s_svc_uuid.u,
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = &s_message_uuid.u,
            .access_cb = prv_message_access,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
          },
          {
            .uuid = &s_counter_uuid.u,
            .access_cb = prv_counter_access,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            .val_handle = &s_counter_handle,
          },
          {},
        },
  },
  {},
};

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_advertise(void) {
  const char *name = ble_svc_gap_device_name();
  struct ble_hs_adv_fields fields = {
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    .name = (const uint8_t *)name,
    .name_len = strlen(name),
    .name_is_complete = 1,
    .uuids128 = &s_svc_uuid,
    .num_uuids128 = 1,
    .uuids128_is_complete = 1,
  };
  const struct ble_gap_adv_params params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };
  int rc;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    printf("peripheral: setting the data failed (%d)\n", rc);
    return;
  }

  rc = ble_gap_adv_start(s_own_addr_type, nullptr, BLE_HS_FOREVER, &params, prv_gap_event, nullptr);
  if (rc != 0) {
    printf("peripheral: advertising failed (%d)\n", rc);
    return;
  }

  printf("peripheral: advertising as %s\n", name);
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        printf("peripheral: connection failed (%d)\n", event->connect.status);
        prv_advertise();
        break;
      }
      printf("peripheral: connected, handle %u\n", event->connect.conn_handle);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      printf("peripheral: disconnected, reason 0x%03x\n", event->disconnect.reason);
      atomic_store(&s_subscribed, false);
      prv_advertise();
      break;
    case BLE_GAP_EVENT_SUBSCRIBE:
      if (event->subscribe.attr_handle == s_counter_handle) {
        atomic_store(&s_subscribed, event->subscribe.cur_notify != 0U);
        printf("peripheral: counter notifications %s\n",
               event->subscribe.cur_notify != 0U ? "on" : "off");
      }
      break;
    case BLE_GAP_EVENT_MTU:
      printf("peripheral: MTU %u\n", event->mtu.value);
      break;
    default:
      break;
  }

  return 0;
}

static void prv_on_sync(void) {
  pbl_sem_give(&s_synced);
}

static void prv_on_reset(int reason) {
  printf("peripheral: host reset, reason %d\n", reason);
}

int main(void) {
  uint8_t addr[6];
  int rc;

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;

  /* The services go in before the host starts. */
  ble_svc_gap_init();
  ble_svc_gatt_init();

  rc = ble_gatts_count_cfg(s_services);
  if (rc == 0) {
    rc = ble_gatts_add_svcs(s_services);
  }
  if (rc == 0) {
    rc = ble_svc_gap_device_name_set(NAME);
  }
  if (rc != 0) {
    printf("peripheral: adding the services failed (%d)\n", rc);
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
    printf("peripheral: no address (%d)\n", rc);
    return rc;
  }

  printf("peripheral: address %02x:%02x:%02x:%02x:%02x:%02x (%s)\n", addr[5], addr[4], addr[3],
         addr[2], addr[1], addr[0], s_own_addr_type == BLE_OWN_ADDR_PUBLIC ? "public" : "random");

  prv_advertise();

  /* Count the seconds, and notify a subscribed peer of each one. */
  while (true) {
    pbl_thread_sleep(PBL_SEC(1));

    atomic_fetch_add(&s_counter, 1U);
    if (atomic_load(&s_subscribed)) {
      ble_gatts_chr_updated(s_counter_handle);
    }
  }

  return 0;
}
