/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/ancs.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/size.h>

static const ble_uuid128_t s_svc_uuid = BLE_UUID128_INIT(PBL_BT_ANCS_UUID_SERVICE);
static const ble_uuid128_t s_ns_uuid = BLE_UUID128_INIT(PBL_BT_ANCS_UUID_NOTIFICATION_SOURCE);
static const ble_uuid128_t s_ds_uuid = BLE_UUID128_INIT(PBL_BT_ANCS_UUID_DATA_SOURCE);
static const ble_uuid128_t s_cp_uuid = BLE_UUID128_INIT(PBL_BT_ANCS_UUID_CONTROL_POINT);

/* The sources come first, as enum pbl_bt_ancs_source has them. */
enum {
  CHR_CONTROL_POINT = PBL_BT_ANCS_SOURCE_DATA + 1,
  CHR_NUM,
};

static const ble_uuid_t *const s_chr_uuids[CHR_NUM] = {
  [PBL_BT_ANCS_SOURCE_NOTIFICATION] = &s_ns_uuid.u,
  [PBL_BT_ANCS_SOURCE_DATA] = &s_ds_uuid.u,
  [CHR_CONTROL_POINT] = &s_cp_uuid.u,
};

enum {
  CMD_GET_NOTIF_ATTRS = 0,
  CMD_GET_APP_ATTRS = 1,
  CMD_PERFORM_ACTION = 2,
};

enum request {
  REQUEST_NONE,
  REQUEST_NOTIF_ATTRS,
  REQUEST_APP_ATTRS,
  REQUEST_ACTION,
};

enum parsed {
  PARSED_INCOMPLETE,
  PARSED_COMPLETE,
  PARSED_BAD,
};

#define NS_LEN 8
/* Command and notification UID. */
#define NOTIF_HDR_LEN 5
/* ID and length. */
#define ATTR_HDR_LEN   3
#define NOTIF_CMD_MAX  (NOTIF_HDR_LEN + PBL_BT_ANCS_CLIENT_ATTRS_MAX * ATTR_HDR_LEN)
#define ACTION_CMD_LEN (NOTIF_HDR_LEN + 1)

static PBL_MUTEX_DEFINE(s_lock);

static struct pbl_bt_ancs_client *prv_client(struct pbl_bt_gattc_service *svc) {
  return (struct pbl_bt_ancs_client *)((uint8_t *)svc - offsetof(struct pbl_bt_ancs_client, svc));
}

static void prv_put_le16(uint8_t *dst, uint16_t value) {
  dst[0] = value & 0xff;
  dst[1] = value >> 8;
}

static void prv_put_le32(uint8_t *dst, uint32_t value) {
  prv_put_le16(dst, value & 0xffff);
  prv_put_le16(dst + 2, value >> 16);
}

static uint16_t prv_get_le16(const uint8_t *src) {
  return src[0] | (src[1] << 8);
}

static uint32_t prv_get_le32(const uint8_t *src) {
  return prv_get_le16(src) | ((uint32_t)prv_get_le16(src + 2) << 16);
}

/* Finds the attributes asked for in what was gathered of the response. */
static enum parsed prv_parse(const struct pbl_bt_ancs_client *client,
                             const struct pbl_bt_ancs_attr **attrs) {
  size_t offset = NOTIF_HDR_LEN;
  bool any = false;

  memset(attrs, 0, client->num_attrs * sizeof(attrs[0]));

  if (client->request == REQUEST_APP_ATTRS) {
    const uint8_t *end = memchr(client->buf + 1, '\0', client->len - 1);

    if (end == nullptr) {
      return PARSED_INCOMPLETE;
    }
    offset = end + 1 - client->buf;
  }

  while (offset + ATTR_HDR_LEN <= client->len) {
    const struct pbl_bt_ancs_attr *attr = (const struct pbl_bt_ancs_attr *)&client->buf[offset];
    const uint16_t len = prv_get_le16(&client->buf[offset + 1]);
    uint8_t i;

    /* The service may send attributes that were not asked for: those are skipped. */
    for (i = 0; i < client->num_attrs; i++) {
      if (client->attrs[i].id == attr->id && attrs[i] == nullptr) {
        break;
      }
    }

    if (i < client->num_attrs && client->attrs[i].max_len != 0 && len > client->attrs[i].max_len) {
      return PARSED_BAD;
    }

    offset += ATTR_HDR_LEN + len;
    if (offset > client->len) {
      return PARSED_INCOMPLETE;
    }

    if (i < client->num_attrs) {
      attrs[i] = attr;
    }
    any = true;
  }

  /* The response does not tell its end: it is taken to end with the attributes it must have. */
  for (uint8_t i = 0; i < client->num_attrs; i++) {
    if (attrs[i] == nullptr && !client->attrs[i].optional) {
      return PARSED_INCOMPLETE;
    }
  }

  return (any && offset == client->len) ? PARSED_COMPLETE : PARSED_INCOMPLETE;
}

/* Ends the request in progress and reports it. Called with the lock held, which it releases. */
static void prv_finish(struct pbl_bt_ancs_client *client, int status) {
  const struct pbl_bt_ancs_attr *attrs[PBL_BT_ANCS_CLIENT_ATTRS_MAX];
  const struct pbl_bt_ancs_client_cb *cb = client->cb;
  const enum request request = client->request;
  const uint8_t num_attrs = client->num_attrs;
  const uint32_t uid = client->uid;

  if (status == 0 && num_attrs > 0 && request != REQUEST_ACTION) {
    (void)prv_parse(client, attrs);
  }
  client->request = REQUEST_NONE;
  pbl_mutex_unlock(&s_lock);

  switch (request) {
    case REQUEST_NOTIF_ATTRS:
      if (cb->notif_attrs != nullptr) {
        cb->notif_attrs(client, status, uid, status == 0 ? attrs : nullptr, num_attrs);
      }
      break;
    case REQUEST_APP_ATTRS:
      if (cb->app_attrs != nullptr) {
        cb->app_attrs(client, status, status == 0 ? attrs : nullptr, num_attrs);
      }
      break;
    case REQUEST_ACTION:
      if (cb->action_performed != nullptr) {
        cb->action_performed(client, status, uid);
      }
      break;
    default:
      break;
  }
}

static void prv_discovered(struct pbl_bt_gattc_service *svc, int status) {
  struct pbl_bt_ancs_client *client = prv_client(svc);

  /* A request made of the service where it was found before gets no answer. */
  pbl_bt_ancs_client_cancel(client);

  if (client->cb->discovered != nullptr) {
    client->cb->discovered(client, status);
  }
}

static void prv_subscribed(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable, int status) {
  struct pbl_bt_ancs_client *client = prv_client(svc);

  if (client->cb->subscribed != nullptr) {
    client->cb->subscribed(client, chr, enable, status);
  }
}

static void prv_notification_source(struct pbl_bt_ancs_client *client, const uint8_t *data,
                                    uint16_t len) {
  struct pbl_bt_ancs_notif notif;

  if (len != NS_LEN || client->cb->notification == nullptr) {
    return;
  }

  notif = (struct pbl_bt_ancs_notif){
    .event = data[0],
    .flags = data[1],
    .category = data[2],
    .category_count = data[3],
    .uid = prv_get_le32(&data[4]),
  };
  client->cb->notification(client, &notif);
}

static void prv_data_source(struct pbl_bt_ancs_client *client, const uint8_t *data, uint16_t len) {
  const struct pbl_bt_ancs_attr *attrs[PBL_BT_ANCS_CLIENT_ATTRS_MAX];

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (client->complete || client->num_attrs == 0 ||
      (client->request != REQUEST_NOTIF_ATTRS && client->request != REQUEST_APP_ATTRS)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  /* A response starts with the command it answers, and with the notification if it has one. */
  if (client->len == 0) {
    bool ours;

    if (client->request == REQUEST_NOTIF_ATTRS) {
      ours = len >= NOTIF_HDR_LEN && data[0] == CMD_GET_NOTIF_ATTRS &&
             prv_get_le32(&data[1]) == client->uid;
    } else {
      ours = len >= 1 && data[0] == CMD_GET_APP_ATTRS;
    }
    if (!ours) {
      prv_finish(client, -EBADMSG);
      return;
    }
  }

  if (len > client->buf_size - client->len) {
    prv_finish(client, -ENOBUFS);
    return;
  }
  memcpy(&client->buf[client->len], data, len);
  client->len += len;

  switch (prv_parse(client, attrs)) {
    case PARSED_BAD:
      prv_finish(client, -EBADMSG);
      return;
    case PARSED_COMPLETE:
      client->complete = true;
      if (client->acked) {
        prv_finish(client, 0);
        return;
      }
      break;
    case PARSED_INCOMPLETE:
      break;
  }

  pbl_mutex_unlock(&s_lock);
}

static void prv_notified(struct pbl_bt_gattc_service *svc, uint8_t chr, const uint8_t *data,
                         uint16_t len) {
  struct pbl_bt_ancs_client *client = prv_client(svc);

  if (chr == PBL_BT_ANCS_SOURCE_NOTIFICATION) {
    prv_notification_source(client, data, len);
  } else if (chr == PBL_BT_ANCS_SOURCE_DATA) {
    prv_data_source(client, data, len);
  }
}

static void prv_written(struct pbl_bt_gattc_service *svc, uint8_t chr, int status) {
  struct pbl_bt_ancs_client *client = prv_client(svc);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (client->request == REQUEST_NONE) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  /* The attributes may come before the answer to the command. */
  client->acked = true;
  if (status != 0 || client->request == REQUEST_ACTION || client->num_attrs == 0 ||
      client->complete) {
    prv_finish(client, status);
    return;
  }

  pbl_mutex_unlock(&s_lock);
}

static const struct pbl_bt_gattc_service_cb s_service_cb = {
  .discovered = prv_discovered,
  .subscribed = prv_subscribed,
  .notified = prv_notified,
  .written = prv_written,
};

int pbl_bt_ancs_client_start(struct pbl_bt_ancs_client *client, uint16_t conn_handle,
                             const struct pbl_bt_ancs_client_cb *cb) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (client->started) {
    pbl_mutex_unlock(&s_lock);
    return -EALREADY;
  }
  client->started = true;
  client->cb = cb;
  client->request = REQUEST_NONE;
  client->svc.svc_uuid = &s_svc_uuid.u;
  client->svc.chr_uuids = s_chr_uuids;
  client->svc.num_chrs = CHR_NUM;
  client->svc.notifying_chrs =
      PBL_BIT(PBL_BT_ANCS_SOURCE_NOTIFICATION) | PBL_BIT(PBL_BT_ANCS_SOURCE_DATA);
  client->svc.chrs = client->chrs;
  pbl_mutex_unlock(&s_lock);

  rc = pbl_bt_gattc_service_start(&client->svc, conn_handle, &s_service_cb);
  if (rc != 0) {
    pbl_bt_ancs_client_stop(client);
  }

  return rc;
}

int pbl_bt_ancs_client_rediscover(struct pbl_bt_ancs_client *client) {
  return pbl_bt_gattc_service_rediscover(&client->svc);
}

void pbl_bt_ancs_client_stop(struct pbl_bt_ancs_client *client) {
  pbl_bt_gattc_service_stop(&client->svc);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  client->started = false;
  client->request = REQUEST_NONE;
  pbl_mutex_unlock(&s_lock);
}

int pbl_bt_ancs_client_subscribe(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                                 bool enable) {
  return pbl_bt_gattc_service_subscribe(&client->svc, source, enable);
}

/* Starts a request by writing its command to the Control Point. */
static int prv_request(struct pbl_bt_ancs_client *client, enum request request, uint32_t uid,
                       const struct pbl_bt_ancs_attr_req *attrs, uint8_t num_attrs,
                       const uint8_t *cmd, uint16_t cmd_len) {
  int rc;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (!client->started) {
    pbl_mutex_unlock(&s_lock);
    return -ENOTCONN;
  }
  if (client->request != REQUEST_NONE) {
    pbl_mutex_unlock(&s_lock);
    return -EBUSY;
  }
  client->request = request;
  client->uid = uid;
  client->acked = false;
  client->complete = false;
  client->len = 0;
  client->num_attrs = num_attrs;
  if (num_attrs > 0) {
    memcpy(client->attrs, attrs, num_attrs * sizeof(attrs[0]));
  }
  pbl_mutex_unlock(&s_lock);

  rc = pbl_bt_gattc_service_write(&client->svc, CHR_CONTROL_POINT, cmd, cmd_len);
  if (rc != 0) {
    pbl_bt_ancs_client_cancel(client);
  }

  return rc;
}

int pbl_bt_ancs_client_get_notif_attrs(struct pbl_bt_ancs_client *client, uint32_t uid,
                                       const struct pbl_bt_ancs_attr_req *attrs,
                                       uint8_t num_attrs) {
  uint8_t cmd[NOTIF_CMD_MAX];
  uint16_t len = NOTIF_HDR_LEN;

  if (num_attrs > PBL_BT_ANCS_CLIENT_ATTRS_MAX) {
    return -EINVAL;
  }

  cmd[0] = CMD_GET_NOTIF_ATTRS;
  prv_put_le32(&cmd[1], uid);
  for (uint8_t i = 0; i < num_attrs; i++) {
    cmd[len++] = attrs[i].id;
    if (attrs[i].id == PBL_BT_ANCS_NOTIF_ATTR_TITLE ||
        attrs[i].id == PBL_BT_ANCS_NOTIF_ATTR_SUBTITLE ||
        attrs[i].id == PBL_BT_ANCS_NOTIF_ATTR_MESSAGE) {
      prv_put_le16(&cmd[len], attrs[i].max_len);
      len += 2;
    }
  }

  return prv_request(client, REQUEST_NOTIF_ATTRS, uid, attrs, num_attrs, cmd, len);
}

int pbl_bt_ancs_client_get_app_attrs(struct pbl_bt_ancs_client *client, const void *app_id,
                                     size_t app_id_len, const struct pbl_bt_ancs_attr_req *attrs,
                                     uint8_t num_attrs) {
  const size_t len = 1 + app_id_len + 1 + num_attrs;
  uint8_t *cmd;
  int rc;

  if (num_attrs == 0 || num_attrs > PBL_BT_ANCS_CLIENT_ATTRS_MAX || len > UINT16_MAX) {
    return -EINVAL;
  }

  cmd = pbl_malloc(len);
  if (cmd == nullptr) {
    return -ENOMEM;
  }

  cmd[0] = CMD_GET_APP_ATTRS;
  memcpy(&cmd[1], app_id, app_id_len);
  cmd[1 + app_id_len] = '\0';
  for (uint8_t i = 0; i < num_attrs; i++) {
    cmd[1 + app_id_len + 1 + i] = attrs[i].id;
  }

  rc = prv_request(client, REQUEST_APP_ATTRS, 0, attrs, num_attrs, cmd, len);
  pbl_free(cmd);

  return rc;
}

int pbl_bt_ancs_client_perform_action(struct pbl_bt_ancs_client *client, uint32_t uid,
                                      enum pbl_bt_ancs_action action) {
  uint8_t cmd[ACTION_CMD_LEN];

  cmd[0] = CMD_PERFORM_ACTION;
  prv_put_le32(&cmd[1], uid);
  cmd[NOTIF_HDR_LEN] = action;

  return prv_request(client, REQUEST_ACTION, uid, nullptr, 0, cmd, sizeof(cmd));
}

void pbl_bt_ancs_client_cancel(struct pbl_bt_ancs_client *client) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  client->request = REQUEST_NONE;
  pbl_mutex_unlock(&s_lock);
}

bool pbl_bt_ancs_client_notify_rx(struct pbl_bt_ancs_client *client, uint16_t conn_handle,
                                  uint16_t attr_handle, struct os_mbuf *om) {
  return pbl_bt_gattc_service_notify_rx(&client->svc, conn_handle, attr_handle, om);
}
