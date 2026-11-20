/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/ams.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/size.h>

static const ble_uuid128_t s_svc_uuid = BLE_UUID128_INIT(PBL_BT_AMS_UUID_SERVICE);
static const ble_uuid128_t s_rc_uuid = BLE_UUID128_INIT(PBL_BT_AMS_UUID_REMOTE_COMMAND);
static const ble_uuid128_t s_eu_uuid = BLE_UUID128_INIT(PBL_BT_AMS_UUID_ENTITY_UPDATE);
static const ble_uuid128_t s_ea_uuid = BLE_UUID128_INIT(PBL_BT_AMS_UUID_ENTITY_ATTRIBUTE);

/* The sources come first, as enum pbl_bt_ams_source has them. */
enum {
  CHR_ENTITY_ATTRIBUTE = PBL_BT_AMS_SOURCE_ENTITY_UPDATE + 1,
  CHR_NUM,
};

static const ble_uuid_t *const s_chr_uuids[CHR_NUM] = {
  [PBL_BT_AMS_SOURCE_REMOTE_COMMAND] = &s_rc_uuid.u,
  [PBL_BT_AMS_SOURCE_ENTITY_UPDATE] = &s_eu_uuid.u,
  [CHR_ENTITY_ATTRIBUTE] = &s_ea_uuid.u,
};

/* Entity, attribute and flags. */
#define UPDATE_HDR_LEN 3

static PBL_MUTEX_DEFINE(s_lock);

static struct pbl_bt_ams_client *prv_client(struct pbl_bt_gattc_service *svc) {
  return (struct pbl_bt_ams_client *)((uint8_t *)svc - offsetof(struct pbl_bt_ams_client, svc));
}

static void prv_cancel_read(struct pbl_bt_ams_client *client) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  client->reading = false;
  pbl_mutex_unlock(&s_lock);
}

/* Ends the read in progress, if any, and reports it. */
static void prv_finish_read(struct pbl_bt_ams_client *client, int status, const uint8_t *value,
                            uint16_t len) {
  uint8_t entity;
  uint8_t attr;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!client->reading) {
    pbl_mutex_unlock(&s_lock);
    return;
  }
  client->reading = false;
  entity = client->entity;
  attr = client->attr;
  pbl_mutex_unlock(&s_lock);

  if (client->cb->attr_read != nullptr) {
    client->cb->attr_read(client, status, entity, attr, status == 0 ? value : nullptr,
                          status == 0 ? len : 0);
  }
}

static void prv_discovered(struct pbl_bt_gattc_service *svc, int status) {
  struct pbl_bt_ams_client *client = prv_client(svc);

  /* A read made of the service where it was found before gets no answer. */
  prv_cancel_read(client);

  if (client->cb->discovered != nullptr) {
    client->cb->discovered(client, status);
  }
}

static void prv_subscribed(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable, int status) {
  struct pbl_bt_ams_client *client = prv_client(svc);

  if (client->cb->subscribed != nullptr) {
    client->cb->subscribed(client, chr, enable, status);
  }
}

static void prv_notified(struct pbl_bt_gattc_service *svc, uint8_t chr, const uint8_t *data,
                         uint16_t len) {
  struct pbl_bt_ams_client *client = prv_client(svc);

  if (chr == PBL_BT_AMS_SOURCE_REMOTE_COMMAND) {
    if (client->cb->commands != nullptr && len <= UINT8_MAX) {
      client->cb->commands(client, data, len);
    }
  } else if (chr == PBL_BT_AMS_SOURCE_ENTITY_UPDATE) {
    struct pbl_bt_ams_update update;

    if (client->cb->update == nullptr || len < UPDATE_HDR_LEN) {
      return;
    }

    update = (struct pbl_bt_ams_update){
      .entity = data[0],
      .attr = data[1],
      .flags = data[2],
      .len = len - UPDATE_HDR_LEN,
      .value = &data[UPDATE_HDR_LEN],
    };
    client->cb->update(client, &update);
  }
}

static void prv_written(struct pbl_bt_gattc_service *svc, uint8_t chr, int status) {
  struct pbl_bt_ams_client *client = prv_client(svc);

  switch (chr) {
    case PBL_BT_AMS_SOURCE_REMOTE_COMMAND:
      if (client->cb->command_sent != nullptr) {
        client->cb->command_sent(client, status);
      }
      break;
    case PBL_BT_AMS_SOURCE_ENTITY_UPDATE:
      if (client->cb->registered != nullptr) {
        client->cb->registered(client, status);
      }
      break;
    case CHR_ENTITY_ATTRIBUTE:
      /* The attribute is chosen: its value can be read. */
      if (status == 0) {
        status = pbl_bt_gattc_service_read(svc, CHR_ENTITY_ATTRIBUTE);
      }
      if (status != 0) {
        prv_finish_read(client, status, nullptr, 0);
      }
      break;
    default:
      break;
  }
}

static void prv_read(struct pbl_bt_gattc_service *svc, uint8_t chr, int status, const uint8_t *data,
                     uint16_t len) {
  prv_finish_read(prv_client(svc), status, data, len);
}

static const struct pbl_bt_gattc_service_cb s_service_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .notified = prv_notified,
  .written = prv_written,
  .read = prv_read,
};

int pbl_bt_ams_client_start(struct pbl_bt_ams_client *client, uint16_t conn_handle,
                            const struct pbl_bt_ams_client_cb *cb) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (client->started) {
    pbl_mutex_unlock(&s_lock);
    return -EALREADY;
  }
  client->started = true;
  client->cb = cb;
  client->reading = false;
  client->svc.svc_uuid = &s_svc_uuid.u;
  client->svc.chr_uuids = s_chr_uuids;
  client->svc.num_chrs = CHR_NUM;
  client->svc.notifying_chrs =
      PBL_BIT(PBL_BT_AMS_SOURCE_REMOTE_COMMAND) | PBL_BIT(PBL_BT_AMS_SOURCE_ENTITY_UPDATE);
  client->svc.chrs = client->chrs;
  pbl_mutex_unlock(&s_lock);

  rc = pbl_bt_gattc_service_start(&client->svc, conn_handle, &s_service_cb);
  if (rc != 0) {
    pbl_bt_ams_client_stop(client);
  }

  return rc;
}

int pbl_bt_ams_client_rediscover(struct pbl_bt_ams_client *client) {
  return pbl_bt_gattc_service_rediscover(&client->svc);
}

void pbl_bt_ams_client_stop(struct pbl_bt_ams_client *client) {
  pbl_bt_gattc_service_stop(&client->svc);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  client->started = false;
  client->reading = false;
  pbl_mutex_unlock(&s_lock);
}

int pbl_bt_ams_client_subscribe(struct pbl_bt_ams_client *client, enum pbl_bt_ams_source source,
                                bool enable) {
  return pbl_bt_gattc_service_subscribe(&client->svc, source, enable);
}

int pbl_bt_ams_client_send_command(struct pbl_bt_ams_client *client,
                                   enum pbl_bt_ams_command command) {
  const uint8_t cmd = command;

  return pbl_bt_gattc_service_write(&client->svc, PBL_BT_AMS_SOURCE_REMOTE_COMMAND, &cmd,
                                    sizeof(cmd));
}

int pbl_bt_ams_client_register(struct pbl_bt_ams_client *client, enum pbl_bt_ams_entity entity,
                               const uint8_t *attrs, uint8_t num_attrs) {
  uint8_t cmd[1 + PBL_BT_AMS_CLIENT_ATTRS_MAX];

  if (num_attrs > PBL_BT_AMS_CLIENT_ATTRS_MAX) {
    return -EINVAL;
  }

  cmd[0] = entity;
  if (num_attrs > 0) {
    memcpy(&cmd[1], attrs, num_attrs);
  }

  return pbl_bt_gattc_service_write(&client->svc, PBL_BT_AMS_SOURCE_ENTITY_UPDATE, cmd,
                                    1 + num_attrs);
}

int pbl_bt_ams_client_read_attr(struct pbl_bt_ams_client *client, enum pbl_bt_ams_entity entity,
                                uint8_t attr) {
  const uint8_t cmd[] = {entity, attr};
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!client->started) {
    pbl_mutex_unlock(&s_lock);
    return -ENOTCONN;
  }
  if (client->reading) {
    pbl_mutex_unlock(&s_lock);
    return -EBUSY;
  }
  client->reading = true;
  client->entity = entity;
  client->attr = attr;
  pbl_mutex_unlock(&s_lock);

  rc = pbl_bt_gattc_service_write(&client->svc, CHR_ENTITY_ATTRIBUTE, cmd, sizeof(cmd));
  if (rc != 0) {
    prv_cancel_read(client);
  }

  return rc;
}

bool pbl_bt_ams_client_notify_rx(struct pbl_bt_ams_client *client, uint16_t conn_handle,
                                 uint16_t attr_handle, struct os_mbuf *om) {
  return pbl_bt_gattc_service_notify_rx(&client->svc, conn_handle, attr_handle, om);
}
