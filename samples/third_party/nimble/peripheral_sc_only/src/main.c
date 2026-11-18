/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/fs/pfs.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_pebble.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define NAME      "PebbleOS SC"
#define MAX_BONDS 4

/* 5e1a0100-7c3b-4f0e-9d2a-6b8c1f4e0a37, and its characteristic. */
#define SAMPLE_UUID(n)                                                                        \
  BLE_UUID128_INIT(0x37, 0x0a, 0x4e, 0x1f, 0x8c, 0x6b, 0x2a, 0x9d, 0x0e, 0x4f, 0x3b, 0x7c, n, \
                   0x01, 0x1a, 0x5e)

static const ble_uuid128_t s_svc_uuid = SAMPLE_UUID(0x00);
static const ble_uuid128_t s_secret_uuid = SAMPLE_UUID(0x01);

static struct pbl_fs_mount s_mount = {.mnt_point = "/", .ops = &pbl_fs_pfs_ops};
static PBL_SEM_DEFINE(s_synced, 0, 1);
static uint8_t s_own_addr_type;

static void prv_print_addr(const char *what, const ble_addr_t *addr) {
  const uint8_t *a = addr->val;

  printf("peripheral_sc_only: %s %02x:%02x:%02x:%02x:%02x:%02x (%s)\n", what, a[5], a[4], a[3],
         a[2], a[1], a[0], addr->type == BLE_ADDR_PUBLIC ? "public" : "random");
}

/* Only readable over a link encrypted with an authenticated key: reading it starts pairing. */
static int prv_secret_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
  static const char secret[] = "Only for bonded peers";

  if (os_mbuf_append(ctxt->om, secret, strlen(secret)) != 0) {
    return BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  printf("peripheral_sc_only: secret read\n");

  return 0;
}

static const struct ble_gatt_svc_def s_services[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &s_svc_uuid.u,
    .characteristics =
        (struct ble_gatt_chr_def[]){
          {
            .uuid = &s_secret_uuid.u,
            .access_cb = prv_secret_access,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN,
          },
          {},
        },
  },
  {},
};

static int prv_gap_event(struct ble_gap_event *event, void *arg);

static void prv_advertise(void) {
  const char *name = ble_svc_gap_device_name();
  /* The 128-bit UUID leaves no room for the name: it goes in the scan response. */
  const struct ble_hs_adv_fields fields = {
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    .uuids128 = &s_svc_uuid,
    .num_uuids128 = 1,
    .uuids128_is_complete = 1,
  };
  const struct ble_hs_adv_fields rsp_fields = {
    .name = (const uint8_t *)name,
    .name_len = strlen(name),
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
    printf("peripheral_sc_only: advertising failed (%d)\n", rc);
    return;
  }

  printf("peripheral_sc_only: advertising as %s\n", name);
}

/* The peer types in the passkey shown here. */
static void prv_show_passkey(uint16_t conn_handle) {
  struct ble_sm_io io = {.action = BLE_SM_IOACT_DISP};
  uint32_t value;
  int rc;

  rc = ble_hs_hci_rand(&value, sizeof(value));
  if (rc != 0) {
    printf("peripheral_sc_only: no random number (%d)\n", rc);
    return;
  }

  io.passkey = value % 1000000U;
  printf("peripheral_sc_only: passkey %06u\n", (unsigned int)io.passkey);

  rc = ble_sm_inject_io(conn_handle, &io);
  if (rc != 0) {
    printf("peripheral_sc_only: passkey refused (%d)\n", rc);
  }
}

static void prv_print_security(uint16_t conn_handle) {
  struct ble_gap_conn_desc desc;

  if (ble_gap_conn_find(conn_handle, &desc) != 0) {
    return;
  }

  printf("peripheral_sc_only: encrypted, %s, %s\n",
         desc.sec_state.authenticated ? "authenticated" : "unauthenticated",
         desc.sec_state.bonded ? "bonded" : "not bonded");
}

/* Runs on the host thread. */
static int prv_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_gap_conn_desc desc;

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status != 0) {
        printf("peripheral_sc_only: connection failed (%d)\n", event->connect.status);
        prv_advertise();
        break;
      }
      printf("peripheral_sc_only: connected\n");
      /* Ask the peer to pair, or to encrypt with the keys of a bond. */
      (void)ble_gap_security_initiate(event->connect.conn_handle);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      printf("peripheral_sc_only: disconnected, reason 0x%03x\n", event->disconnect.reason);
      prv_advertise();
      break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        prv_show_passkey(event->passkey.conn_handle);
      }
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if (event->enc_change.status != 0) {
        printf("peripheral_sc_only: encryption failed (%d)\n", event->enc_change.status);
        break;
      }
      prv_print_security(event->enc_change.conn_handle);
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      /* A bonded peer that lost its keys: forget ours, and pair again. */
      if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
        prv_print_addr("pairing again with", &desc.peer_id_addr);
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

static void prv_on_reset(int reason) {
  printf("peripheral_sc_only: host reset, reason %d\n", reason);
}

int main(void) {
  ble_addr_t bonds[MAX_BONDS];
  int num_bonds;
  int rc;

  /* The store keeps the bonds in a settings file. */
  rc = pbl_fs_mount(&s_mount);
  if (rc != 0) {
    printf("peripheral_sc_only: mount failed (%d)\n", rc);
    return rc;
  }

  nimble_port_init();
  ble_hs_cfg.sync_cb = prv_on_sync;
  ble_hs_cfg.reset_cb = prv_on_reset;
  /* A new bond replaces the oldest one when the store is full. */
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();

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
    printf("peripheral_sc_only: adding the services failed (%d)\n", rc);
    return rc;
  }

  rc = ble_store_util_bonded_peers(bonds, &num_bonds, MAX_BONDS);
  if (rc != 0) {
    printf("peripheral_sc_only: reading the bonds failed (%d)\n", rc);
    return rc;
  }

  printf("peripheral_sc_only: %d bonded peer%s\n", num_bonds, num_bonds == 1 ? "" : "s");
  for (int i = 0; i < num_bonds; i++) {
    prv_print_addr("bonded with", &bonds[i]);
  }

  nimble_port_pebble_init();

  pbl_sem_take(&s_synced, PBL_FOREVER);

  rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  }
  if (rc != 0) {
    printf("peripheral_sc_only: no address (%d)\n", rc);
    return rc;
  }

  prv_advertise();

  return 0;
}
