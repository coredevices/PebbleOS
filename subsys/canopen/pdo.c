/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>
#include <string.h>

#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>

#define SUB_COB_ID     1U
#define SUB_TYPE       2U
#define SUB_INHIBIT    3U
#define SUB_EVENT      5U
#define SUB_SYNC_START 6U

#define TYPE_SYNC_MAX 240U
#define TYPE_EVENT    254U

enum prv_kind {
  KIND_NONE,
  KIND_RPDO_COMM,
  KIND_RPDO_MAP,
  KIND_TPDO_COMM,
  KIND_TPDO_MAP,
};

static uint16_t prv_comm_index(const struct pbl_canopen__pdo *pdo, bool tx) {
  return (uint16_t)((tx ? PBL_CANOPEN__IDX_TPDO_COMM : PBL_CANOPEN__IDX_RPDO_COMM) + pdo->num - 1U);
}

static uint32_t prv_comm(struct pbl_canopen *co, const struct pbl_canopen__pdo *pdo, bool tx,
                         uint8_t sub) {
  return pbl_canopen__od_get_u32(co->cfg.od, prv_comm_index(pdo, tx), sub);
}

static enum prv_kind prv_kind(struct pbl_canopen *co, uint16_t index, uint16_t *n) {
  const struct pbl_canopen_od *od = co->cfg.od;
  static const struct {
    uint16_t base;
    enum prv_kind kind;
  } ranges[] = {
    {PBL_CANOPEN__IDX_RPDO_COMM, KIND_RPDO_COMM},
    {PBL_CANOPEN__IDX_RPDO_MAP, KIND_RPDO_MAP},
    {PBL_CANOPEN__IDX_TPDO_COMM, KIND_TPDO_COMM},
    {PBL_CANOPEN__IDX_TPDO_MAP, KIND_TPDO_MAP},
  };

  for (size_t i = 0U; i < 4U; i++) {
    bool tx = ranges[i].kind == KIND_TPDO_COMM || ranges[i].kind == KIND_TPDO_MAP;
    uint16_t num = tx ? od->num_tpdos : od->num_rpdos;

    if (index >= ranges[i].base && index < ranges[i].base + num) {
      *n = (uint16_t)(index - ranges[i].base);
      return ranges[i].kind;
    }
  }

  return KIND_NONE;
}

static bool prv_restricted(uint32_t id) {
  return id <= 0x07fU || (id >= 0x101U && id <= 0x180U) || (id >= 0x581U && id <= 0x5ffU) ||
         (id >= 0x601U && id <= 0x67fU) || (id >= 0x6e0U && id <= 0x6ffU) ||
         (id >= 0x701U && id <= 0x7ffU);
}

static bool prv_is_dummy(uint16_t index) {
  return index >= PBL_CANOPEN_TYPE_BOOLEAN && index <= PBL_CANOPEN_TYPE_UNSIGNED32;
}

static uint8_t prv_dummy_size(uint16_t index) {
  static const uint8_t sizes[] = {1U, 1U, 2U, 4U, 1U, 2U, 4U};

  return sizes[index - 1U];
}

/* Resolve the first @p count entries of mapping @p map_index, into @p pdo if not NULL. */
static uint32_t prv_resolve(struct pbl_canopen *co, uint16_t map_index, bool tx, uint8_t count,
                            struct pbl_canopen__pdo *pdo) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t attr = tx ? PBL_CANOPEN_ATTR_TPDO : PBL_CANOPEN_ATTR_RPDO;
  size_t total = 0U;

  if (count > PBL_CANOPEN_PDO_MAX_MAP) {
    return PBL_CANOPEN__ABORT_VALUE;
  }

  for (uint8_t i = 0U; i < count; i++) {
    uint32_t m = pbl_canopen__od_get_u32(od, map_index, i + 1U);
    uint16_t index = (uint16_t)(m >> 16);
    uint8_t subindex = (uint8_t)(m >> 8);
    uint8_t bits = (uint8_t)m;
    const struct pbl_canopen_od_entry *e = nullptr;
    uint8_t len;

    if (bits == 0U || (bits % 8U) != 0U) {
      return PBL_CANOPEN__ABORT_NO_MAP;
    }
    len = bits / 8U;

    if (!tx && prv_is_dummy(index)) {
      if (prv_dummy_size(index) != len) {
        return PBL_CANOPEN__ABORT_NO_MAP;
      }
    } else {
      e = pbl_canopen__od_find(od, index, subindex);
      if (e == nullptr) {
        return PBL_CANOPEN__ABORT_NO_OBJECT;
      }
      if ((e->attr & attr) == 0U || e->size != len) {
        return PBL_CANOPEN__ABORT_NO_MAP;
      }
    }

    total += len;
    if (total > PBL_CAN_MAX_LEN) {
      return PBL_CANOPEN__ABORT_MAP_LEN;
    }

    if (pdo != nullptr) {
      pdo->map[i] = e;
      pdo->map_len[i] = len;
    }
  }

  if (pdo != nullptr) {
    pdo->num_map = count;
    pdo->len = (uint8_t)total;
  }

  return 0U;
}

static void prv_tpdo_stop(struct pbl_canopen__tpdo *t) {
  pbl_delayed_work_cancel(&t->pdo.timer);
  pbl_delayed_work_cancel(&t->inhibit);
  t->sync_count = 0U;
  t->sync_started = false;
  t->event = false;
  t->inhibited = false;
}

static void prv_rpdo_stop(struct pbl_canopen__rpdo *r) {
  pbl_delayed_work_cancel(&r->pdo.timer);
  r->buffered = false;
  r->monitored = false;
}

static void prv_tpdo_send(struct pbl_canopen *co, struct pbl_canopen__tpdo *t) {
  struct pbl_canopen__pdo *pdo = &t->pdo;
  uint16_t inhibit = (uint16_t)prv_comm(co, pdo, true, SUB_INHIBIT);
  uint16_t event = (uint16_t)prv_comm(co, pdo, true, SUB_EVENT);
  uint8_t data[PBL_CAN_MAX_LEN];
  uint8_t off = 0U;

  for (uint8_t i = 0U; i < pdo->num_map; i++) {
    pbl_canopen__od_to_wire(pdo->map[i], &data[off], pdo->map_len[i]);
    off += pdo->map_len[i];
  }

  pbl_canopen__send(co, pdo->can_id | (pdo->ext ? PBL_CANOPEN__COB_EXT : 0U), data, pdo->len);
  t->event = false;

  if (pdo->type >= TYPE_EVENT) {
    if (inhibit != 0U) {
      t->inhibited = true;
      pbl_delayed_work_reschedule(&pbl_canopen__workq, &t->inhibit,
                                  PBL_TICKS(PBL_US_TO_TICKS_CEIL(inhibit * 100U)));
    }
    if (event != 0U) {
      pbl_delayed_work_reschedule(&pbl_canopen__workq, &pdo->timer, PBL_MSEC(event));
    }
  }
}

static bool prv_tpdo_active(struct pbl_canopen *co, const struct pbl_canopen__tpdo *t) {
  return co->started && co->nmt == PBL_CANOPEN_NMT_OPERATIONAL && t->pdo.valid;
}

static void prv_tpdo_event(struct pbl_canopen *co, struct pbl_canopen__tpdo *t) {
  if (!prv_tpdo_active(co, t)) {
    return;
  }

  if (t->pdo.type == 0U || t->inhibited) {
    t->event = true;
  } else if (t->pdo.type >= TYPE_EVENT) {
    prv_tpdo_send(co, t);
  }
}

static void prv_tpdo_timer(struct pbl_work *work) {
  struct pbl_canopen__tpdo *t = PBL_CONTAINER_OF(work, struct pbl_canopen__tpdo, pdo.timer.work);
  struct pbl_canopen *co = t->pdo.co;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (prv_tpdo_active(co, t) && t->pdo.type >= TYPE_EVENT) {
    if (t->inhibited) {
      t->event = true;
    } else {
      prv_tpdo_send(co, t);
    }
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_tpdo_inhibit(struct pbl_work *work) {
  struct pbl_canopen__tpdo *t = PBL_CONTAINER_OF(work, struct pbl_canopen__tpdo, inhibit.work);
  struct pbl_canopen *co = t->pdo.co;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  t->inhibited = false;
  if (prv_tpdo_active(co, t) && t->event && t->pdo.type >= TYPE_EVENT) {
    prv_tpdo_send(co, t);
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_rpdo_timer(struct pbl_work *work) {
  struct pbl_canopen__rpdo *r = PBL_CONTAINER_OF(work, struct pbl_canopen__rpdo, pdo.timer.work);
  struct pbl_canopen *co = r->pdo.co;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->started && co->nmt == PBL_CANOPEN_NMT_OPERATIONAL && r->pdo.valid && r->monitored) {
    uint8_t msef[5] = {0};

    r->monitored = false;
    pbl_put_le16(r->pdo.num, msef);
    (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_RPDO_TIMEOUT, PBL_CANOPEN__ERR_REG_COMM,
                                  msef);
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_config(struct pbl_canopen *co, struct pbl_canopen__pdo *pdo, bool tx) {
  uint16_t map_index =
      (uint16_t)((tx ? PBL_CANOPEN__IDX_TPDO_MAP : PBL_CANOPEN__IDX_RPDO_MAP) + pdo->num - 1U);
  uint32_t cob = prv_comm(co, pdo, tx, SUB_COB_ID);
  uint8_t count = (uint8_t)pbl_canopen__od_get_u32(co->cfg.od, map_index, 0U);

  pdo->can_id = cob & PBL_CAN_EXT_ID_MASK;
  pdo->ext = (cob & PBL_CANOPEN__COB_EXT) != 0U;
  pdo->type = (uint8_t)prv_comm(co, pdo, tx, SUB_TYPE);
  pdo->valid =
      (cob & PBL_CANOPEN__COB_INVALID) == 0U && prv_resolve(co, map_index, tx, count, pdo) == 0U;

  if (tx) {
    struct pbl_canopen__tpdo *t = PBL_CONTAINER_OF(pdo, struct pbl_canopen__tpdo, pdo);

    prv_tpdo_stop(t);
    if (prv_tpdo_active(co, t) && pdo->type >= TYPE_EVENT) {
      prv_tpdo_send(co, t);
    }
  } else {
    prv_rpdo_stop(PBL_CONTAINER_OF(pdo, struct pbl_canopen__rpdo, pdo));
  }
}

void pbl_canopen__pdo_init(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    struct pbl_canopen__rpdo *r = &od->rpdos[i];

    memset(r, 0, sizeof(*r));
    r->pdo.co = co;
    r->pdo.num = i + 1U;
    pbl_delayed_work_init(&r->pdo.timer, prv_rpdo_timer);
  }

  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    struct pbl_canopen__tpdo *t = &od->tpdos[i];

    memset(t, 0, sizeof(*t));
    t->pdo.co = co;
    t->pdo.num = i + 1U;
    pbl_delayed_work_init(&t->pdo.timer, prv_tpdo_timer);
    pbl_delayed_work_init(&t->inhibit, prv_tpdo_inhibit);
  }
}

void pbl_canopen__pdo_config_all(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    prv_config(co, &od->rpdos[i].pdo, false);
  }
  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    prv_config(co, &od->tpdos[i].pdo, true);
  }
}

void pbl_canopen__pdo_stop(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    prv_rpdo_stop(&od->rpdos[i]);
    od->rpdos[i].pdo.valid = false;
  }
  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    prv_tpdo_stop(&od->tpdos[i]);
    od->tpdos[i].pdo.valid = false;
  }
}

void pbl_canopen__pdo_nmt(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    prv_rpdo_stop(&od->rpdos[i]);
  }

  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    struct pbl_canopen__tpdo *t = &od->tpdos[i];

    prv_tpdo_stop(t);
    if (prv_tpdo_active(co, t) && t->pdo.type >= TYPE_EVENT) {
      prv_tpdo_send(co, t);
    }
  }
}

uint32_t pbl_canopen__pdo_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                const void *val) {
  uint32_t v = *(const uint32_t *)val;
  uint16_t n;
  enum prv_kind kind = prv_kind(co, e->index, &n);
  bool tx = kind == KIND_TPDO_COMM || kind == KIND_TPDO_MAP;
  uint16_t comm_index =
      (uint16_t)((tx ? PBL_CANOPEN__IDX_TPDO_COMM : PBL_CANOPEN__IDX_RPDO_COMM) + n);
  uint32_t cob = pbl_canopen__od_get_u32(co->cfg.od, comm_index, SUB_COB_ID);
  bool valid = (cob & PBL_CANOPEN__COB_INVALID) == 0U;

  switch (kind) {
    case KIND_RPDO_COMM:
    case KIND_TPDO_COMM:
      switch (e->subindex) {
        case SUB_COB_ID: {
          uint32_t id = v & PBL_CAN_EXT_ID_MASK;
          bool ext = (v & PBL_CANOPEN__COB_EXT) != 0U;
          bool new_valid = (v & PBL_CANOPEN__COB_INVALID) == 0U;

          if (!ext && id > PBL_CAN_STD_ID_MASK) {
            return PBL_CANOPEN__ABORT_VALUE;
          }
          if (valid && new_valid &&
              (cob & ~PBL_CANOPEN__COB_NO_RTR) != (v & ~PBL_CANOPEN__COB_NO_RTR)) {
            return PBL_CANOPEN__ABORT_VALUE;
          }
          if (new_valid && !ext && prv_restricted(id)) {
            return PBL_CANOPEN__ABORT_VALUE;
          }
          return 0U;
        }
        case SUB_TYPE:
          return (v <= TYPE_SYNC_MAX || v >= TYPE_EVENT) ? 0U : PBL_CANOPEN__ABORT_VALUE;
        case SUB_INHIBIT:
        case SUB_SYNC_START:
          return valid ? PBL_CANOPEN__ABORT_VALUE : 0U;
        default:
          return 0U;
      }
    case KIND_RPDO_MAP:
    case KIND_TPDO_MAP:
      if (valid) {
        return PBL_CANOPEN__ABORT_DATA_STATE;
      }
      if (e->subindex == 0U) {
        return prv_resolve(co, e->index, tx, (uint8_t)v, nullptr);
      }
      return pbl_canopen__od_get_u32(co->cfg.od, e->index, 0U) != 0U ? PBL_CANOPEN__ABORT_DATA_STATE
                                                                     : 0U;
    default:
      return 0U;
  }
}

void pbl_canopen__pdo_written(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint16_t n;

  switch (prv_kind(co, e->index, &n)) {
    case KIND_RPDO_COMM:
    case KIND_RPDO_MAP:
      prv_config(co, &od->rpdos[n].pdo, false);
      break;
    case KIND_TPDO_COMM:
    case KIND_TPDO_MAP:
      prv_config(co, &od->tpdos[n].pdo, true);
      break;
    default:
      break;
  }
}

void pbl_canopen__tpdo_changed(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    struct pbl_canopen__tpdo *t = &od->tpdos[i];

    if (t->pdo.type > TYPE_SYNC_MAX || t->pdo.type == 0U) {
      for (uint8_t j = 0U; j < t->pdo.num_map; j++) {
        if (t->pdo.map[j] == e) {
          prv_tpdo_event(co, t);
          break;
        }
      }
    }
  }
}

int pbl_canopen__tpdo_request(struct pbl_canopen *co, uint16_t num) {
  const struct pbl_canopen_od *od = co->cfg.od;
  struct pbl_canopen__tpdo *t;

  if (num == 0U || num > od->num_tpdos) {
    return -ENOENT;
  }

  t = &od->tpdos[num - 1U];
  if (t->pdo.type != 0U && t->pdo.type <= TYPE_SYNC_MAX) {
    return -ENOTSUP;
  }

  prv_tpdo_event(co, t);

  return 0;
}

static void prv_rpdo_apply(struct pbl_canopen *co, struct pbl_canopen__rpdo *r,
                           const uint8_t *data) {
  uint8_t off = 0U;

  for (uint8_t i = 0U; i < r->pdo.num_map; i++) {
    if (r->pdo.map[i] != nullptr) {
      (void)pbl_canopen__od_write_bus(co, r->pdo.map[i], &data[off], r->pdo.map_len[i]);
    }
    off += r->pdo.map_len[i];
  }
}

bool pbl_canopen__pdo_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  const struct pbl_canopen_od *od = co->cfg.od;
  bool ext = (f->flags & PBL_CAN_FRAME_IDE) != 0U;

  if (co->nmt != PBL_CANOPEN_NMT_OPERATIONAL) {
    return false;
  }

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    struct pbl_canopen__rpdo *r = &od->rpdos[i];
    uint16_t event;
    uint8_t msef[5] = {0};

    if (!r->pdo.valid || r->pdo.ext != ext || r->pdo.can_id != f->id) {
      continue;
    }

    pbl_put_le16(r->pdo.num, msef);
    if (f->len < r->pdo.len) {
      (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_PDO_LEN, PBL_CANOPEN__ERR_REG_COMM, msef);
      return true;
    }
    if (f->len > r->pdo.len) {
      (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_PDO_LEN_EXCESS, PBL_CANOPEN__ERR_REG_COMM,
                                    msef);
    } else {
      (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_PDO_LEN_EXCESS);
    }
    (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_PDO_LEN);

    event = (uint16_t)prv_comm(co, &r->pdo, false, SUB_EVENT);
    if (event != 0U) {
      r->monitored = true;
      pbl_delayed_work_reschedule(&pbl_canopen__workq, &r->pdo.timer, PBL_MSEC(event));
      (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_RPDO_TIMEOUT);
    }

    if (r->pdo.type <= TYPE_SYNC_MAX) {
      memcpy(r->buf, f->data, r->pdo.len);
      r->buffered = true;
    } else {
      prv_rpdo_apply(co, r, f->data);
    }

    return true;
  }

  return false;
}

void pbl_canopen__sync_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  const struct pbl_canopen_od *od = co->cfg.od;
  const struct pbl_canopen_callbacks *cb = co->cfg.cb;
  uint8_t counter = f->len == 1U ? f->data[0] : 0U;

  if (co->nmt != PBL_CANOPEN_NMT_OPERATIONAL || f->len > 1U) {
    return;
  }

  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    struct pbl_canopen__rpdo *r = &od->rpdos[i];

    if (r->buffered) {
      r->buffered = false;
      prv_rpdo_apply(co, r, r->buf);
    }
  }

  if (cb != nullptr && cb->sync != nullptr) {
    cb->sync(co, counter, f->cycles, co->cfg.arg);
  }

  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    struct pbl_canopen__tpdo *t = &od->tpdos[i];
    uint8_t type = t->pdo.type;

    if (!t->pdo.valid || type > TYPE_SYNC_MAX) {
      continue;
    }

    if (type == 0U) {
      if (t->event) {
        prv_tpdo_send(co, t);
      }
      continue;
    }

    if (!t->sync_started) {
      uint8_t start = (uint8_t)prv_comm(co, &t->pdo, true, SUB_SYNC_START);

      t->sync_started = start == 0U || f->len == 0U || counter == start;
      t->sync_count = 0U;
      if (!t->sync_started) {
        continue;
      }
      if (start != 0U && f->len == 1U) {
        prv_tpdo_send(co, t);
        continue;
      }
    }

    if (++t->sync_count >= type) {
      t->sync_count = 0U;
      prv_tpdo_send(co, t);
    }
  }
}
