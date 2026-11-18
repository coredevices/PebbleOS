/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/bluetooth/clients/ancs.h>
#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/util/size.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define NAME "PebbleOS ANCS"

#define TITLE_MAX   32
#define MESSAGE_MAX 96

static struct pbl_fs_mount s_mount = {.mnt_point = "/", .ops = &pbl_fs_pfs_ops};
static PBL_SEM_DEFINE(s_synced, 0, 1);
static uint8_t s_own_addr_type;

static const struct pbl_bt_ancs_attr_req s_attrs[] = {
  {.id = PBL_BT_ANCS_NOTIF_ATTR_TITLE, .max_len = TITLE_MAX},
  {.id = PBL_BT_ANCS_NOTIF_ATTR_MESSAGE, .max_len = MESSAGE_MAX},
};

/* Room for the response: its header, and each attribute with its own. */
PBL_BT_ANCS_CLIENT_DEFINE(s_client,
                          5 + 2 * sizeof(struct pbl_bt_ancs_attr) + TITLE_MAX + MESSAGE_MAX);

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_discovered(struct pbl_bt_ancs_client *client, int status) {
  if (status != 0) {
    printf("ancs_client: no ANCS on the peer (%d)\n", status);
    return;
  }

  printf("ancs_client: found ANCS\n");
  /* The Data Source first, so that no response is missed. */
  (void)pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_DATA, true);
  (void)pbl_bt_ancs_client_subscribe(client, PBL_BT_ANCS_SOURCE_NOTIFICATION, true);
}

static void prv_subscribed(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                           bool enable, int status) {
  if (status != 0) {
    printf("ancs_client: subscribing failed (%d)\n", status);
  } else if (source == PBL_BT_ANCS_SOURCE_NOTIFICATION) {
    printf("ancs_client: subscribed\n");
  }
}

static void prv_notification(struct pbl_bt_ancs_client *client,
                             const struct pbl_bt_ancs_notif *notif) {
  static const char *const events[] = {"added", "modified", "removed"};

  if (notif->event >= ARRAY_LENGTH(events)) {
    return;
  }

  printf("ancs_client: notification %u %s, category %u%s\n", (unsigned int)notif->uid,
         events[notif->event], notif->category,
         (notif->flags & PBL_BT_ANCS_FLAG_PRE_EXISTING) ? ", from before" : "");

  /* One request at a time: the attributes of a notification that comes meanwhile are not got. */
  if (notif->event != PBL_BT_ANCS_EVENT_REMOVED) {
    (void)pbl_bt_ancs_client_get_notif_attrs(client, notif->uid, s_attrs, ARRAY_LENGTH(s_attrs));
  }
}

static void prv_notif_attrs(struct pbl_bt_ancs_client *client, int status, uint32_t uid,
                            const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  static const char *const names[] = {"title", "message"};

  if (status != 0) {
    /* E.g. the notification is gone since. */
    printf("ancs_client: request refused (%d)\n", status);
    return;
  }

  for (uint8_t i = 0; i < num_attrs; i++) {
    printf("ancs_client:   %s: %.*s\n", names[i], (int)attrs[i]->len,
           (const char *)attrs[i]->value);
  }
}

static const struct pbl_bt_ancs_client_cb s_ancs_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .notification = prv_notification,
  .notif_attrs = prv_notif_attrs,
};

static void prv_advertise(void) {
  /*
   * Flags, and a solicitation for ANCS: iOS lists accessories that ask for it. The payload is
   * built by hand, as NimBLE's advertising fields have no solicitation: each field is its
   * length (type and data), its type and its data.
   */
  static const uint8_t data[] = {
    1 + 1,  BLE_HS_ADV_TYPE_FLAGS,        BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    1 + 16, BLE_HS_ADV_TYPE_SOL_UUIDS128, PBL_BT_ANCS_UUID_SERVICE,
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

  rc = ble_gap_adv_set_data(data, sizeof(data));
  if (rc == 0) {
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  }
  if (rc == 0) {
    rc = ble_gap_adv_start(s_own_addr_type, nullptr, BLE_HS_FOREVER, &params, prv_gap_event,
                           nullptr);
  }
  if (rc != 0) {
    printf("ancs_client: advertising failed (%d)\n", rc);
    return;
  }

  printf("ancs_client: advertising as %s\n", NAME);
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_gap_conn_desc desc;
  int rc;

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        prv_advertise();
        break;
      }
      printf("ancs_client: connected\n");
      /* ANCS is only there for a paired accessory, over an encrypted link. */
      (void)ble_gap_security_initiate(event->connect.conn_handle);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      printf("ancs_client: disconnected, reason 0x%03x\n", event->disconnect.reason);
      pbl_bt_ancs_client_stop(&s_client);
      prv_advertise();
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if (event->enc_change.status != 0) {
        printf("ancs_client: encryption failed (%d)\n", event->enc_change.status);
        break;
      }
      rc = pbl_bt_ancs_client_start(&s_client, event->enc_change.conn_handle, &s_ancs_cb);
      if (rc != 0) {
        printf("ancs_client: starting the client failed (%d)\n", rc);
      }
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      (void)pbl_bt_gattc_notify_rx(event->notify_rx.conn_handle, event->notify_rx.attr_handle,
                                   event->notify_rx.om);
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

  /* The store keeps the bond in a settings file. */
  rc = pbl_fs_mount(&s_mount);
  if (rc != 0) {
    printf("ancs_client: mount failed (%d)\n", rc);
    return rc;
  }

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();

  ble_svc_gap_init();
  ble_svc_gatt_init();

  rc = ble_svc_gap_device_name_set(NAME);
  if (rc != 0) {
    printf("ancs_client: setting the name failed (%d)\n", rc);
    return rc;
  }

  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  }
  if (rc != 0) {
    printf("ancs_client: no address (%d)\n", rc);
    return rc;
  }

  prv_advertise();

  return 0;
}
