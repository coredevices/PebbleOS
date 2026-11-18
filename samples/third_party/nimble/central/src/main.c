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

#define CCCD_UUID     0x2902U
#define MESSAGE       "Hello from central"
#define MESSAGE_MAX   20U
#define NOTIFICATIONS 3

/* The service of the peripheral sample, and its characteristics. */
#define SAMPLE_UUID(n)                                                                        \
  BLE_UUID128_INIT(0x37, 0x0a, 0x4e, 0x1f, 0x8c, 0x6b, 0x2a, 0x9d, 0x0e, 0x4f, 0x3b, 0x7c, n, \
                   0x00, 0x1a, 0x5e)

static const ble_uuid128_t s_svc_uuid = SAMPLE_UUID(0x00);
static const ble_uuid128_t s_message_uuid = SAMPLE_UUID(0x01);
static const ble_uuid128_t s_counter_uuid = SAMPLE_UUID(0x02);

static PBL_SEM_DEFINE(s_synced, 0, 1);
static PBL_SEM_DEFINE(s_done, 0, 1);
static uint8_t s_own_addr_type;

/* Only the host thread touches these. */
static uint16_t s_svc_end_handle;
static uint16_t s_message_handle;
static uint16_t s_counter_handle;
static uint16_t s_cccd_handle;
static bool s_written;
static int s_notifications;

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_scan(void) {
  const struct ble_gap_disc_params params = {
    .filter_duplicates = 1,
  };
  int rc;

  rc = ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &params, prv_gap_event, nullptr);
  if (rc != 0) {
    printf("central: scanning failed (%d)\n", rc);
    return;
  }

  printf("central: scanning for the peripheral sample\n");
}

static bool prv_is_sample(const struct ble_gap_disc_desc *desc) {
  struct ble_hs_adv_fields fields;

  if (ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data) != 0) {
    return false;
  }

  for (uint8_t i = 0U; i < fields.num_uuids128; i++) {
    if (ble_uuid_cmp(&fields.uuids128[i].u, &s_svc_uuid.u) == 0) {
      return true;
    }
  }

  return false;
}

static void prv_fail(uint16_t conn_handle, const char *what, int rc) {
  printf("central: %s failed (%d)\n", what, rc);
  (void)ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

static int prv_on_subscribed(uint16_t conn_handle, const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg) {
  if (error->status != 0) {
    prv_fail(conn_handle, "subscribing", error->status);
    return 0;
  }

  printf("central: subscribed to the counter\n");

  return 0;
}

static int prv_on_dsc(uint16_t conn_handle, const struct ble_gatt_error *error,
                      uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {
  static const uint8_t notify[] = {0x01U, 0x00U};
  int rc;

  if (error->status == 0) {
    if (s_cccd_handle == 0U && ble_uuid_u16(&dsc->uuid.u) == CCCD_UUID) {
      s_cccd_handle = dsc->handle;
    }
    return 0;
  }

  if (error->status != BLE_HS_EDONE || s_cccd_handle == 0U) {
    prv_fail(conn_handle, "finding the CCCD", error->status);
    return 0;
  }

  rc = ble_gattc_write_flat(conn_handle, s_cccd_handle, notify, sizeof(notify), prv_on_subscribed,
                            nullptr);
  if (rc != 0) {
    prv_fail(conn_handle, "subscribing", rc);
  }

  return 0;
}

static int prv_on_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr, void *arg);

/* Read before and after writing it; then on to the counter. */
static int prv_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr, void *arg) {
  char message[MESSAGE_MAX + 1U];
  uint16_t len = 0U;
  int rc;

  if (error->status != 0) {
    prv_fail(conn_handle, "reading the message", error->status);
    return 0;
  }

  (void)ble_hs_mbuf_to_flat(attr->om, message, MESSAGE_MAX, &len);
  message[len] = '\0';
  printf("central: the message is \"%s\"\n", message);

  if (!s_written) {
    s_written = true;
    rc = ble_gattc_write_flat(conn_handle, s_message_handle, MESSAGE, strlen(MESSAGE),
                              prv_on_written, nullptr);
    if (rc != 0) {
      prv_fail(conn_handle, "writing the message", rc);
    }
    return 0;
  }

  /* The CCCD is among the descriptors after the counter's value. */
  rc =
      ble_gattc_disc_all_dscs(conn_handle, s_counter_handle, s_svc_end_handle, prv_on_dsc, nullptr);
  if (rc != 0) {
    prv_fail(conn_handle, "finding the CCCD", rc);
  }

  return 0;
}

static int prv_on_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr, void *arg) {
  int rc;

  if (error->status != 0) {
    prv_fail(conn_handle, "writing the message", error->status);
    return 0;
  }

  rc = ble_gattc_read(conn_handle, s_message_handle, prv_on_read, nullptr);
  if (rc != 0) {
    prv_fail(conn_handle, "reading the message", rc);
  }

  return 0;
}

static int prv_on_chr(uint16_t conn_handle, const struct ble_gatt_error *error,
                      const struct ble_gatt_chr *chr, void *arg) {
  int rc;

  if (error->status == 0) {
    if (ble_uuid_cmp(&chr->uuid.u, &s_message_uuid.u) == 0) {
      s_message_handle = chr->val_handle;
    } else if (ble_uuid_cmp(&chr->uuid.u, &s_counter_uuid.u) == 0) {
      s_counter_handle = chr->val_handle;
    }
    return 0;
  }

  if (error->status != BLE_HS_EDONE || s_message_handle == 0U || s_counter_handle == 0U) {
    prv_fail(conn_handle, "finding the characteristics", error->status);
    return 0;
  }

  rc = ble_gattc_read(conn_handle, s_message_handle, prv_on_read, nullptr);
  if (rc != 0) {
    prv_fail(conn_handle, "reading the message", rc);
  }

  return 0;
}

static int prv_on_svc(uint16_t conn_handle, const struct ble_gatt_error *error,
                      const struct ble_gatt_svc *service, void *arg) {
  int rc;

  if (error->status == 0) {
    s_svc_end_handle = service->end_handle;
    rc = ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                                 prv_on_chr, nullptr);
    if (rc != 0) {
      prv_fail(conn_handle, "finding the characteristics", rc);
    }
    return 0;
  }

  if (error->status != BLE_HS_EDONE || s_svc_end_handle == 0U) {
    prv_fail(conn_handle, "finding the service", error->status);
  }

  return 0;
}

static void prv_on_counter(uint16_t conn_handle, struct os_mbuf *om) {
  uint8_t value[4];
  uint16_t len = 0U;

  (void)ble_hs_mbuf_to_flat(om, value, sizeof(value), &len);
  if (len != sizeof(value)) {
    return;
  }

  printf("central: counter %u\n", (unsigned int)get_le32(value));

  if (++s_notifications == NOTIFICATIONS) {
    (void)ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  }
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  int rc;

  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
      if (!prv_is_sample(&event->disc)) {
        break;
      }
      (void)ble_gap_disc_cancel();
      rc = ble_gap_connect(s_own_addr_type, &event->disc.addr, 30000, nullptr, prv_gap_event,
                           nullptr);
      if (rc != 0) {
        printf("central: connecting failed (%d)\n", rc);
        prv_scan();
      }
      break;
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        printf("central: connection failed (%d)\n", event->connect.status);
        prv_scan();
        break;
      }
      printf("central: connected\n");
      s_svc_end_handle = 0U;
      s_message_handle = 0U;
      s_counter_handle = 0U;
      s_cccd_handle = 0U;
      s_written = false;
      rc = ble_gattc_disc_svc_by_uuid(event->connect.conn_handle, &s_svc_uuid.u, prv_on_svc,
                                      nullptr);
      if (rc != 0) {
        prv_fail(event->connect.conn_handle, "finding the service", rc);
      }
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      if (event->notify_rx.attr_handle == s_counter_handle) {
        prv_on_counter(event->notify_rx.conn_handle, event->notify_rx.om);
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      printf("central: disconnected, reason 0x%03x\n", event->disconnect.reason);
      if (s_notifications >= NOTIFICATIONS) {
        pbl_sem_give(&s_done);
      } else {
        prv_scan();
      }
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
  printf("central: host reset, reason %d\n", reason);
}

int main(void) {
  int rc;

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;
  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  }
  if (rc != 0) {
    printf("central: no address (%d)\n", rc);
    return rc;
  }

  prv_scan();

  pbl_sem_take(&s_done, PBL_FOREVER);

  printf("Sample done\n");

  return 0;
}
