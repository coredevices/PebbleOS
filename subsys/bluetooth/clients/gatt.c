/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "op_queue.h"

#include <errno.h>
#include <string.h>

#include <pbl/bluetooth/clients/gatt.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>

#include <host/ble_hs.h>

struct discovery {
  uint16_t conn_handle;
  const ble_uuid_t *svc_uuid;
  const ble_uuid_t *const *chr_uuids;
  uint8_t num_chrs;
  pbl_bt_gattc_discover_cb_t cb;
  void *arg;
  uint16_t start_handle;
  uint16_t end_handle;
  /* Last handle of each characteristic asked for: its descriptors come up to it. */
  uint16_t *chr_end_handles;
  /* The characteristic asked for that was found last and whose end is not known yet. */
  uint8_t open_chr;
  struct pbl_bt_gattc_chr chrs[];
};

#define NO_CHR UINT8_MAX

/* The queue frees the discovery, which is its op's context: call back first. */
static void prv_done(struct discovery *d, int status) {
  d->cb(d->conn_handle, status, d->chrs, d->arg);
  nimble_gattc_op_queue_complete();
}

static int prv_dsc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                      uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {
  static const ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);
  struct discovery *d = arg;

  if (error->status == BLE_HS_EDONE) {
    prv_done(d, 0);
    return 0;
  } else if (error->status != 0) {
    prv_done(d, error->status);
    return 0;
  }

  if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) != 0) {
    return 0;
  }

  /* The descriptor is of the characteristic it comes within: others have theirs in between. */
  for (uint8_t i = 0; i < d->num_chrs; i++) {
    struct pbl_bt_gattc_chr *chr = &d->chrs[i];

    if (chr->val_handle != 0 && chr->cccd_handle == 0 && chr->val_handle < dsc->handle &&
        dsc->handle <= d->chr_end_handles[i]) {
      chr->cccd_handle = dsc->handle;
      break;
    }
  }

  return 0;
}

static int prv_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                      const struct ble_gatt_chr *chr, void *arg) {
  struct discovery *d = arg;
  int rc;

  if (error->status == 0) {
    /* A characteristic ends where the next one of the service is declared. */
    if (d->open_chr != NO_CHR) {
      d->chr_end_handles[d->open_chr] = chr->def_handle - 1;
      d->open_chr = NO_CHR;
    }
    for (uint8_t i = 0; i < d->num_chrs; i++) {
      if (d->chrs[i].val_handle == 0 && ble_uuid_cmp(&chr->uuid.u, d->chr_uuids[i]) == 0) {
        d->chrs[i].val_handle = chr->val_handle;
        d->chrs[i].properties = chr->properties;
        d->chr_end_handles[i] = d->end_handle;
        d->open_chr = i;
        break;
      }
    }
    return 0;
  } else if (error->status != BLE_HS_EDONE) {
    prv_done(d, error->status);
    return 0;
  }

  if (d->start_handle >= d->end_handle) {
    prv_done(d, 0);
    return 0;
  }

  /* All the attributes of the service at once: the descriptors are picked from them. */
  rc = ble_gattc_disc_all_dscs(conn_handle, d->start_handle, d->end_handle, prv_dsc_cb, d);
  if (rc != 0) {
    prv_done(d, rc);
  }

  return 0;
}

static int prv_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                      const struct ble_gatt_svc *svc, void *arg) {
  struct discovery *d = arg;
  int rc;

  if (error->status == 0) {
    /* The first one, should the peer have several. */
    if (d->start_handle == 0) {
      d->start_handle = svc->start_handle;
      d->end_handle = svc->end_handle;
    }
    return 0;
  } else if (error->status != BLE_HS_EDONE) {
    prv_done(d, error->status);
    return 0;
  }

  if (d->start_handle == 0) {
    prv_done(d, BLE_HS_ENOENT);
    return 0;
  }

  rc = ble_gattc_disc_all_chrs(conn_handle, d->start_handle, d->end_handle, prv_chr_cb, d);
  if (rc != 0) {
    prv_done(d, rc);
  }

  return 0;
}

static int prv_start(void *ctx) {
  struct discovery *d = ctx;
  int rc;

  rc = ble_gattc_disc_svc_by_uuid(d->conn_handle, d->svc_uuid, prv_svc_cb, d);
  if (rc != 0) {
    d->cb(d->conn_handle, rc, d->chrs, d->arg);
  }

  return rc;
}

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_bt_gattc_notify_listener, bt_gattc_ntf);

bool pbl_bt_gattc_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_bt_gattc_notify_listener, bt_gattc_ntf, l) {
    if (l->fn(conn_handle, attr_handle, om)) {
      return true;
    }
  }

  return false;
}

int pbl_bt_gattc_discover(uint16_t conn_handle, const ble_uuid_t *svc_uuid,
                          const ble_uuid_t *const *chr_uuids, uint8_t num_chrs,
                          pbl_bt_gattc_discover_cb_t cb, void *arg) {
  struct discovery *d;

  d = pbl_zalloc(sizeof(*d) + num_chrs * (sizeof(d->chrs[0]) + sizeof(d->chr_end_handles[0])));
  if (d == nullptr) {
    return BLE_HS_ENOMEM;
  }

  d->chr_end_handles = (uint16_t *)&d->chrs[num_chrs];
  d->open_chr = NO_CHR;

  d->conn_handle = conn_handle;
  d->svc_uuid = svc_uuid;
  d->chr_uuids = chr_uuids;
  d->num_chrs = num_chrs;
  d->cb = cb;
  d->arg = arg;

  nimble_gattc_op_queue_push(prv_start, d);

  return 0;
}

static PBL_MUTEX_DEFINE(s_service_lock);

enum service_op_type {
  SERVICE_OP_SUBSCRIBE,
  SERVICE_OP_UNSUBSCRIBE,
  SERVICE_OP_WRITE,
  SERVICE_OP_READ,
};

/* What an answer of the peer is for. */
struct service_op {
  struct pbl_bt_gattc_service *svc;
  uint32_t generation;
  /* Of a rediscovery, which outlives the service moving. */
  uint32_t session;
  uint8_t chr;
  enum service_op_type type;
  /* What was read so far. */
  uint8_t *value;
  uint16_t len;
};

static int prv_status(int rc) {
  if (rc == 0 || rc == BLE_HS_EDONE) {
    return 0;
  } else if (rc > BLE_HS_ERR_ATT_BASE && rc <= BLE_HS_ERR_ATT_BASE + UINT8_MAX) {
    return rc - BLE_HS_ERR_ATT_BASE;
  }

  switch (rc) {
    case BLE_HS_ENOENT:
      return -ENOENT;
    case BLE_HS_ENOTCONN:
      return -ENOTCONN;
    case BLE_HS_ENOMEM:
      return -ENOMEM;
    case BLE_HS_ETIMEOUT:
      return -ETIMEDOUT;
    default:
      return -EIO;
  }
}

/* The callbacks if the answer is for the service as it was started last, null otherwise. */
static const struct pbl_bt_gattc_service_cb *prv_service_cb(struct pbl_bt_gattc_service *svc,
                                                            uint32_t generation) {
  const struct pbl_bt_gattc_service_cb *cb = nullptr;

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  if (svc->started && svc->generation == generation) {
    cb = svc->cb;
  }
  pbl_mutex_unlock(&s_service_lock);

  return cb;
}

/* Whether a service that was found has the characteristics its client needs. */
static int prv_check_chrs(const struct pbl_bt_gattc_service *svc, int status,
                          const struct pbl_bt_gattc_chr *chrs) {
  for (uint8_t i = 0; status == 0 && i < svc->num_chrs; i++) {
    const uint32_t bit = 1UL << i;

    if (chrs[i].val_handle == 0 && (svc->optional_chrs & bit) == 0) {
      status = BLE_HS_ENOENT;
    } else if (chrs[i].val_handle != 0 && chrs[i].cccd_handle == 0 &&
               (svc->notifying_chrs & bit) != 0) {
      status = BLE_HS_ENOENT;
    }
  }

  return status;
}

static void prv_service_discovered(uint16_t conn_handle, int status,
                                   const struct pbl_bt_gattc_chr *chrs, void *arg) {
  struct service_op *op = arg;
  struct pbl_bt_gattc_service *svc = op->svc;
  const struct pbl_bt_gattc_service_cb *cb = nullptr;

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  if (svc->started && svc->generation == op->generation) {
    cb = svc->cb;
    status = prv_check_chrs(svc, status, chrs);
    if (status == 0) {
      memcpy(svc->chrs, chrs, svc->num_chrs * sizeof(chrs[0]));
    }
  }
  pbl_mutex_unlock(&s_service_lock);

  pbl_free(op);

  if (cb != nullptr && cb->discovered != nullptr) {
    cb->discovered(svc, prv_status(status));
  }
}

int pbl_bt_gattc_service_start(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                               const struct pbl_bt_gattc_service_cb *cb) {
  struct service_op *op;
  int rc;

  op = pbl_malloc(sizeof(*op));
  if (op == nullptr) {
    return -ENOMEM;
  }

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  if (svc->started) {
    pbl_mutex_unlock(&s_service_lock);
    pbl_free(op);
    return -EALREADY;
  }
  svc->started = true;
  svc->conn_handle = conn_handle;
  svc->cb = cb;
  memset(svc->chrs, 0, svc->num_chrs * sizeof(svc->chrs[0]));
  svc->session++;
  *op = (struct service_op){.svc = svc, .generation = ++svc->generation};
  pbl_mutex_unlock(&s_service_lock);

  rc = pbl_bt_gattc_discover(conn_handle, svc->svc_uuid, svc->chr_uuids, svc->num_chrs,
                             prv_service_discovered, op);
  if (rc != 0) {
    pbl_free(op);
    pbl_bt_gattc_service_stop(svc);
  }

  return prv_status(rc);
}

static void prv_service_rediscovered(uint16_t conn_handle, int status,
                                     const struct pbl_bt_gattc_chr *chrs, void *arg) {
  struct service_op *op = arg;
  struct pbl_bt_gattc_service *svc = op->svc;
  const struct pbl_bt_gattc_service_cb *cb = nullptr;

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  if (svc->started && svc->session == op->session) {
    bool was_found = false;

    for (uint8_t i = 0; i < svc->num_chrs; i++) {
      was_found = was_found || svc->chrs[i].val_handle != 0;
    }

    status = prv_check_chrs(svc, status, chrs);
    if (status == 0 && memcmp(svc->chrs, chrs, svc->num_chrs * sizeof(chrs[0])) != 0) {
      memcpy(svc->chrs, chrs, svc->num_chrs * sizeof(chrs[0]));
      cb = svc->cb;
    } else if (status != 0 && was_found) {
      memset(svc->chrs, 0, svc->num_chrs * sizeof(svc->chrs[0]));
      cb = svc->cb;
    }
    /* What was asked of the service where it was gets no answer. */
    if (cb != nullptr) {
      svc->generation++;
    }
  }
  pbl_mutex_unlock(&s_service_lock);

  pbl_free(op);

  if (cb != nullptr && cb->discovered != nullptr) {
    cb->discovered(svc, prv_status(status));
  }
}

int pbl_bt_gattc_service_rediscover(struct pbl_bt_gattc_service *svc) {
  struct service_op *op;
  uint16_t conn_handle;
  bool started;
  int rc;

  op = pbl_malloc(sizeof(*op));
  if (op == nullptr) {
    return -ENOMEM;
  }

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  started = svc->started;
  conn_handle = svc->conn_handle;
  *op = (struct service_op){.svc = svc, .session = svc->session};
  pbl_mutex_unlock(&s_service_lock);

  if (!started) {
    pbl_free(op);
    return -ENOTCONN;
  }

  rc = pbl_bt_gattc_discover(conn_handle, svc->svc_uuid, svc->chr_uuids, svc->num_chrs,
                             prv_service_rediscovered, op);
  if (rc != 0) {
    pbl_free(op);
  }

  return prv_status(rc);
}

void pbl_bt_gattc_service_stop(struct pbl_bt_gattc_service *svc) {
  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  svc->started = false;
  memset(svc->chrs, 0, svc->num_chrs * sizeof(svc->chrs[0]));
  pbl_mutex_unlock(&s_service_lock);
}

static int prv_service_op_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg) {
  struct service_op *op = arg;
  struct pbl_bt_gattc_service *svc = op->svc;
  const struct pbl_bt_gattc_service_cb *cb = prv_service_cb(svc, op->generation);
  int status = prv_status(error->status);
  /* Other than 0 ends a read that has more to come. */
  int rc = 0;

  if (cb == nullptr) {
    rc = BLE_HS_EDONE;
    goto done;
  }

  switch (op->type) {
    case SERVICE_OP_SUBSCRIBE:
    case SERVICE_OP_UNSUBSCRIBE:
      if (cb->subscribed != nullptr) {
        cb->subscribed(svc, op->chr, op->type == SERVICE_OP_SUBSCRIBE, status);
      }
      break;
    case SERVICE_OP_WRITE:
      if (cb->written != nullptr) {
        cb->written(svc, op->chr, status);
      }
      break;
    case SERVICE_OP_READ:
      /* The value comes in parts, and then that it is done. */
      if (error->status == 0 && attr != nullptr) {
        const uint16_t len = (attr->om != nullptr) ? OS_MBUF_PKTLEN(attr->om) : 0;
        uint8_t *value;

        if (len == 0) {
          return 0;
        }
        value = pbl_realloc(op->value, op->len + len);
        if (value != nullptr) {
          os_mbuf_copydata(attr->om, 0, len, &value[op->len]);
          op->value = value;
          op->len += len;
          return 0;
        }
        status = -ENOMEM;
        rc = BLE_HS_EDONE;
      }
      if (cb->read != nullptr) {
        cb->read(svc, op->chr, status, status == 0 ? op->value : nullptr,
                 status == 0 ? op->len : 0);
      }
      break;
  }

done:
  pbl_free(op->value);
  pbl_free(op);

  return rc;
}

/* Starts an operation on a characteristic, on its CCCD for the subscriptions. */
static int prv_service_op(struct pbl_bt_gattc_service *svc, uint8_t chr, enum service_op_type type,
                          const void *data, uint16_t len) {
  const bool on_cccd = (type == SERVICE_OP_SUBSCRIBE || type == SERVICE_OP_UNSUBSCRIBE);
  const uint16_t cccd = htole16(type == SERVICE_OP_SUBSCRIBE ? BLE_GATT_CCCD_NOTIFY : 0);
  struct service_op *op;
  uint16_t conn_handle;
  uint16_t handle = 0;
  int rc;

  op = pbl_malloc(sizeof(*op));
  if (op == nullptr) {
    return -ENOMEM;
  }

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  conn_handle = svc->conn_handle;
  if (svc->started && chr < svc->num_chrs) {
    handle = on_cccd ? svc->chrs[chr].cccd_handle : svc->chrs[chr].val_handle;
  }
  *op = (struct service_op){.svc = svc, .generation = svc->generation, .chr = chr, .type = type};
  pbl_mutex_unlock(&s_service_lock);

  if (handle == 0) {
    pbl_free(op);
    return -ENOTCONN;
  }

  if (on_cccd) {
    rc = ble_gattc_write_flat(conn_handle, handle, &cccd, sizeof(cccd), prv_service_op_cb, op);
  } else if (type == SERVICE_OP_WRITE) {
    rc = ble_gattc_write_flat(conn_handle, handle, data, len, prv_service_op_cb, op);
  } else {
    rc = ble_gattc_read_long(conn_handle, handle, 0, prv_service_op_cb, op);
  }
  if (rc != 0) {
    pbl_free(op);
  }

  return prv_status(rc);
}

int pbl_bt_gattc_service_subscribe(struct pbl_bt_gattc_service *svc, uint8_t chr, bool enable) {
  return prv_service_op(svc, chr, enable ? SERVICE_OP_SUBSCRIBE : SERVICE_OP_UNSUBSCRIBE, nullptr,
                        0);
}

int pbl_bt_gattc_service_write(struct pbl_bt_gattc_service *svc, uint8_t chr, const void *data,
                               uint16_t len) {
  return prv_service_op(svc, chr, SERVICE_OP_WRITE, data, len);
}

int pbl_bt_gattc_service_read(struct pbl_bt_gattc_service *svc, uint8_t chr) {
  return prv_service_op(svc, chr, SERVICE_OP_READ, nullptr, 0);
}

bool pbl_bt_gattc_service_notify_rx(struct pbl_bt_gattc_service *svc, uint16_t conn_handle,
                                    uint16_t attr_handle, struct os_mbuf *om) {
  const struct pbl_bt_gattc_service_cb *cb = nullptr;
  const uint16_t len = OS_MBUF_PKTLEN(om);
  uint8_t chr = UINT8_MAX;
  uint8_t *flat;

  pbl_mutex_lock(&s_service_lock, PBL_FOREVER);
  if (svc->started && svc->conn_handle == conn_handle) {
    for (uint8_t i = 0; i < svc->num_chrs; i++) {
      if (svc->chrs[i].val_handle == attr_handle) {
        chr = i;
        cb = svc->cb;
        break;
      }
    }
  }
  pbl_mutex_unlock(&s_service_lock);

  if (chr == UINT8_MAX) {
    return false;
  }
  if (cb == nullptr || cb->notified == nullptr) {
    return true;
  }

  if (om->om_len == len) {
    cb->notified(svc, chr, om->om_data, len);
    return true;
  }

  flat = pbl_malloc(len);
  if (flat != nullptr) {
    os_mbuf_copydata(om, 0, len, flat);
    cb->notified(svc, chr, flat, len);
    pbl_free(flat);
  }

  return true;
}
