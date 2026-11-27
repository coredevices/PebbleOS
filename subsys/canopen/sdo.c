/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <string.h>

#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#define CCS_DOWNLOAD_SEGMENT 0U
#define CCS_DOWNLOAD_INIT    1U
#define CCS_UPLOAD_INIT      2U
#define CCS_UPLOAD_SEGMENT   3U
#define CCS_ABORT            4U

#define SCS_UPLOAD_SEGMENT   0U
#define SCS_DOWNLOAD_SEGMENT 1U
#define SCS_UPLOAD_INIT      2U
#define SCS_DOWNLOAD_INIT    3U
#define SCS_ABORT            4U

#define SDO_E      PBL_BIT(1)
#define SDO_S      PBL_BIT(0)
#define SDO_C      PBL_BIT(0)
#define SDO_TOGGLE PBL_BIT(4)

enum {
  SDO_IDLE,
  SDO_DOWNLOAD,
  SDO_UPLOAD,
};

static void prv_respond(struct pbl_canopen *co, const uint8_t data[8]) {
  pbl_canopen__send(co, PBL_CANOPEN__COB_SDO_TX + co->cfg.node_id, data, 8U);
}

static void prv_end(struct pbl_canopen *co) {
  co->sdo.state = SDO_IDLE;
  co->sdo.entry = nullptr;
  pbl_delayed_work_cancel(&co->sdo.timeout);
}

static void prv_abort(struct pbl_canopen *co, uint16_t index, uint8_t subindex, uint32_t code) {
  uint8_t data[8] = {SCS_ABORT << 5};

  pbl_put_le16(index, &data[1]);
  data[3] = subindex;
  pbl_put_le32(code, &data[4]);
  prv_respond(co, data);
  prv_end(co);
}

static void prv_abort_entry(struct pbl_canopen *co, uint32_t code) {
  const struct pbl_canopen_od_entry *e = co->sdo.entry;

  prv_abort(co, e != nullptr ? e->index : 0U, e != nullptr ? e->subindex : 0U, code);
}

static void prv_timeout(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, sdo.timeout.work);

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->started && co->sdo.state != SDO_IDLE) {
    prv_abort_entry(co, PBL_CANOPEN__ABORT_TIMEOUT);
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_arm(struct pbl_canopen *co) {
  pbl_delayed_work_reschedule(&pbl_canopen__workq, &co->sdo.timeout,
                              PBL_MSEC(CONFIG_CANOPEN_SDO_TIMEOUT_MS));
}

static uint32_t prv_lookup(struct pbl_canopen *co, uint16_t index, uint8_t subindex,
                           const struct pbl_canopen_od_entry **e) {
  *e = pbl_canopen__od_find(co->cfg.od, index, subindex);
  if (*e != nullptr) {
    return 0U;
  }

  return pbl_canopen__od_has_index(co->cfg.od, index) ? PBL_CANOPEN__ABORT_NO_SUB
                                                      : PBL_CANOPEN__ABORT_NO_OBJECT;
}

static void prv_download_init(struct pbl_canopen *co, const uint8_t *req, uint16_t index,
                              uint8_t subindex) {
  const struct pbl_canopen_od_entry *e;
  uint8_t data[8] = {SCS_DOWNLOAD_INIT << 5};
  uint32_t abort = prv_lookup(co, index, subindex, &e);

  if (abort == 0U && (e->attr & PBL_CANOPEN_ATTR_WRITE) == 0U) {
    abort = PBL_CANOPEN__ABORT_READ_ONLY;
  }
  if (abort != 0U) {
    prv_abort(co, index, subindex, abort);
    return;
  }

  co->sdo.entry = e;
  if ((req[0] & SDO_E) != 0U) {
    size_t len = (req[0] & SDO_S) != 0U ? 4U - ((req[0] >> 2) & 0x3U) : PBL_MIN(e->size, 4U);

    abort = pbl_canopen__od_write_bus(co, e, &req[4], len);
    if (abort != 0U) {
      prv_abort(co, index, subindex, abort);
      return;
    }
  } else {
    uint32_t size = (req[0] & SDO_S) != 0U ? pbl_get_le32(&req[4]) : e->size;

    if (size > e->size) {
      prv_abort(co, index, subindex, PBL_CANOPEN__ABORT_TYPE_LEN_HIGH);
      return;
    }
    co->sdo.state = SDO_DOWNLOAD;
    co->sdo.size = (req[0] & SDO_S) != 0U ? size : 0U;
    co->sdo.offset = 0U;
    co->sdo.toggle = false;
    prv_arm(co);
  }

  memcpy(&data[1], &req[1], 3U);
  prv_respond(co, data);
}

static void prv_download_segment(struct pbl_canopen *co, const uint8_t *req) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t data[8] = {SCS_DOWNLOAD_SEGMENT << 5};
  size_t len = 7U - ((req[0] >> 1) & 0x7U);
  bool toggle = (req[0] & SDO_TOGGLE) != 0U;
  uint32_t abort;

  if (co->sdo.state != SDO_DOWNLOAD) {
    prv_abort(co, 0U, 0U, PBL_CANOPEN__ABORT_CMD);
    return;
  }
  if (toggle != co->sdo.toggle) {
    prv_abort_entry(co, PBL_CANOPEN__ABORT_TOGGLE);
    return;
  }
  if (co->sdo.offset + len > co->sdo.entry->size || co->sdo.offset + len > od->buf_size) {
    prv_abort_entry(co, PBL_CANOPEN__ABORT_TYPE_LEN_HIGH);
    return;
  }

  memcpy(&od->buf[co->sdo.offset], &req[1], len);
  co->sdo.offset += len;

  if ((req[0] & SDO_C) != 0U) {
    if (co->sdo.size != 0U && co->sdo.size != co->sdo.offset) {
      prv_abort_entry(co, PBL_CANOPEN__ABORT_TYPE_LEN);
      return;
    }
    abort = pbl_canopen__od_write_bus(co, co->sdo.entry, od->buf, co->sdo.offset);
    if (abort != 0U) {
      prv_abort_entry(co, abort);
      return;
    }
    prv_end(co);
  } else {
    co->sdo.toggle = !toggle;
    prv_arm(co);
  }

  data[0] |= toggle ? SDO_TOGGLE : 0U;
  prv_respond(co, data);
}

static void prv_upload_init(struct pbl_canopen *co, const uint8_t *req, uint16_t index,
                            uint8_t subindex) {
  const struct pbl_canopen_od *od = co->cfg.od;
  const struct pbl_canopen_od_entry *e;
  uint8_t data[8] = {SCS_UPLOAD_INIT << 5};
  uint32_t abort = prv_lookup(co, index, subindex, &e);
  size_t len;

  if (abort == 0U && (e->attr & PBL_CANOPEN_ATTR_READ) == 0U) {
    abort = PBL_CANOPEN__ABORT_WRITE_ONLY;
  }
  if (abort != 0U) {
    prv_abort(co, index, subindex, abort);
    return;
  }

  memcpy(&data[1], &req[1], 3U);
  len = pbl_canopen__od_len(e);
  if (len <= 4U && len > 0U) {
    data[0] |= (uint8_t)(((4U - len) << 2) | SDO_E | SDO_S);
    pbl_canopen__od_to_wire(e, &data[4], len);
  } else {
    if (len > od->buf_size) {
      prv_abort(co, index, subindex, PBL_CANOPEN__ABORT_GENERAL);
      return;
    }
    pbl_canopen__od_to_wire(e, od->buf, len);
    data[0] |= SDO_S;
    pbl_put_le32((uint32_t)len, &data[4]);
    co->sdo.entry = e;
    co->sdo.state = SDO_UPLOAD;
    co->sdo.size = (uint32_t)len;
    co->sdo.offset = 0U;
    co->sdo.toggle = false;
    prv_arm(co);
  }

  prv_respond(co, data);
}

static void prv_upload_segment(struct pbl_canopen *co, const uint8_t *req) {
  uint8_t data[8] = {SCS_UPLOAD_SEGMENT << 5};
  bool toggle = (req[0] & SDO_TOGGLE) != 0U;
  size_t len;

  if (co->sdo.state != SDO_UPLOAD) {
    prv_abort(co, 0U, 0U, PBL_CANOPEN__ABORT_CMD);
    return;
  }
  if (toggle != co->sdo.toggle) {
    prv_abort_entry(co, PBL_CANOPEN__ABORT_TOGGLE);
    return;
  }

  len = PBL_MIN(co->sdo.size - co->sdo.offset, 7U);
  memcpy(&data[1], &co->cfg.od->buf[co->sdo.offset], len);
  co->sdo.offset += len;
  data[0] |= (uint8_t)((toggle ? SDO_TOGGLE : 0U) | ((7U - len) << 1));

  if (co->sdo.offset == co->sdo.size) {
    data[0] |= SDO_C;
    prv_end(co);
  } else {
    co->sdo.toggle = !toggle;
    prv_arm(co);
  }

  prv_respond(co, data);
}

void pbl_canopen__sdo_init(struct pbl_canopen *co) {
  pbl_delayed_work_init(&co->sdo.timeout, prv_timeout);
}

void pbl_canopen__sdo_reset(struct pbl_canopen *co) {
  prv_end(co);
}

void pbl_canopen__sdo_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  const uint8_t *req = f->data;
  uint16_t index = pbl_get_le16(&req[1]);
  uint8_t subindex = req[3];
  uint8_t ccs = req[0] >> 5;

  if (f->len != 8U) {
    return;
  }

  if (ccs == CCS_ABORT) {
    prv_end(co);
    return;
  }

  if ((ccs == CCS_DOWNLOAD_INIT || ccs == CCS_UPLOAD_INIT) && co->sdo.state != SDO_IDLE) {
    prv_end(co);
  }

  switch (ccs) {
    case CCS_DOWNLOAD_INIT:
      prv_download_init(co, req, index, subindex);
      break;
    case CCS_DOWNLOAD_SEGMENT:
      prv_download_segment(co, req);
      break;
    case CCS_UPLOAD_INIT:
      prv_upload_init(co, req, index, subindex);
      break;
    case CCS_UPLOAD_SEGMENT:
      prv_upload_segment(co, req);
      break;
    default:
      prv_abort(co, index, subindex, PBL_CANOPEN__ABORT_CMD);
      break;
  }
}
