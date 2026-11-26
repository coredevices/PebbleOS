/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DEFINE(canopen, CONFIG_CANOPEN_LOG_LEVEL);

#define NMT_CS_START      0x01U
#define NMT_CS_STOP       0x02U
#define NMT_CS_PRE_OP     0x80U
#define NMT_CS_RESET_NODE 0x81U
#define NMT_CS_RESET_COMM 0x82U

#define ERR_BEHAV_PRE_OP  0U
#define ERR_BEHAV_STOPPED 2U

PBL_WORKQ_DEFINE(pbl_canopen__workq);
static struct pbl_thread s_thread;
PBL_THREAD_STACK_DEFINE(s_stack, CONFIG_CANOPEN_THREAD_STACK_SIZE);
static PBL_MUTEX_DEFINE(s_thread_lock);
static bool s_thread_started;

static void prv_tx_kick(struct pbl_canopen *co);

static void prv_tx_done(const struct pbl_can *can, int status, void *arg) {
  struct pbl_canopen *co = arg;

  pbl_work_submit(&pbl_canopen__workq, &co->tx_work);
}

static void prv_tx_kick(struct pbl_canopen *co) {
  while (co->tx_count > 0U) {
    const struct pbl_canopen__frame *f = &co->tx_buf[co->tx_head];
    struct pbl_can_frame frame = {
      .id = f->id,
      .flags = f->flags,
      .len = f->len,
    };
    int ret;

    memcpy(frame.data, f->data, f->len);
    ret = pbl_can_send(co->cfg.can, &frame, PBL_NO_WAIT, prv_tx_done, co);
    if (ret == -EAGAIN) {
      break;
    }
    if (ret != 0) {
      PBL_LOG_DBG("frame %03" PRIx32 " not sent (%d)", f->id, ret);
    }

    co->tx_head = (uint8_t)((co->tx_head + 1U) % CONFIG_CANOPEN_TX_QUEUE_SIZE);
    co->tx_count--;
  }
}

static void prv_tx_work(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, tx_work);

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->started) {
    prv_tx_kick(co);
  }
  pbl_mutex_unlock(&co->lock);
}

void pbl_canopen__send(struct pbl_canopen *co, uint32_t id, const uint8_t *data, uint8_t len) {
  struct pbl_canopen__frame *f;

  if (co->tx_count == CONFIG_CANOPEN_TX_QUEUE_SIZE) {
    co->tx_overruns++;
    return;
  }

  f = &co->tx_buf[(co->tx_head + co->tx_count) % CONFIG_CANOPEN_TX_QUEUE_SIZE];
  f->id = id & PBL_CAN_EXT_ID_MASK;
  f->flags = (id & PBL_CANOPEN__COB_EXT) != 0U ? PBL_CAN_FRAME_IDE : 0U;
  f->len = len;
  memcpy(f->data, data, len);
  co->tx_count++;

  prv_tx_kick(co);
}

/* Heartbeat */

static void prv_hb_send(struct pbl_canopen *co) {
  uint8_t state = (uint8_t)co->nmt;

  pbl_canopen__send(co, PBL_CANOPEN__COB_HEARTBEAT + co->cfg.node_id, &state, 1U);
}

static void prv_hb_work(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, heartbeat.work);
  uint32_t period;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  period = pbl_canopen__od_get_u32(co->cfg.od, PBL_CANOPEN__IDX_HB_PRODUCER, 0U);
  if (co->started && period != 0U) {
    prv_hb_send(co);
    pbl_delayed_work_schedule_next(&co->heartbeat, pbl_ms_to_ticks(period));
  }
  pbl_mutex_unlock(&co->lock);
}

static void prv_hb_notify(struct pbl_canopen *co, struct pbl_canopen__hb_consumer *c,
                          enum pbl_canopen_hb_event event) {
  const struct pbl_canopen_callbacks *cb = co->cfg.cb;

  if (cb != nullptr && cb->heartbeat != nullptr) {
    cb->heartbeat(co, c->node_id, event, (enum pbl_canopen_nmt_state)c->state, co->cfg.arg);
  }
}

static void prv_hb_timeout(struct pbl_work *work) {
  struct pbl_canopen__hb_consumer *c =
      PBL_CONTAINER_OF(work, struct pbl_canopen__hb_consumer, timeout.work);
  struct pbl_canopen *co = c->co;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->started && c->alive && c->time_ms != 0U) {
    const uint8_t msef[5] = {c->node_id};

    c->alive = false;
    PBL_LOG_DBG("node %u heartbeat lost", c->node_id);
    (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_HEARTBEAT, PBL_CANOPEN__ERR_REG_COMM, msef);
    prv_hb_notify(co, c, PBL_CANOPEN_HB_TIMEOUT);
    pbl_canopen__comm_error(co);
  }
  pbl_mutex_unlock(&co->lock);
}

static bool prv_hb_any_lost(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;

  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    const struct pbl_canopen__hb_consumer *c = &od->hb_consumers[i];

    if (c->time_ms != 0U && !c->alive && c->state != PBL_CANOPEN_NMT_INITIALIZING) {
      return true;
    }
  }

  return false;
}

static void prv_hb_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t node_id = (uint8_t)(f->id - PBL_CANOPEN__COB_HEARTBEAT);

  if (f->len != 1U) {
    return;
  }

  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    struct pbl_canopen__hb_consumer *c = &od->hb_consumers[i];
    uint8_t state = f->data[0] & 0x7fU;
    bool was_lost;

    if (c->time_ms == 0U || c->node_id != node_id) {
      continue;
    }

    was_lost = !c->alive && c->state != PBL_CANOPEN_NMT_INITIALIZING;
    c->alive = true;
    pbl_delayed_work_reschedule(&pbl_canopen__workq, &c->timeout, PBL_MSEC(c->time_ms));

    if (state == PBL_CANOPEN_NMT_INITIALIZING) {
      c->state = state;
      prv_hb_notify(co, c, PBL_CANOPEN_HB_BOOT);
    } else if (state != c->state) {
      c->state = state;
      prv_hb_notify(co, c, PBL_CANOPEN_HB_STATE);
    }

    if (was_lost && !prv_hb_any_lost(co)) {
      (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_HEARTBEAT);
    }
  }
}

uint32_t pbl_canopen__hb_consumer_check(struct pbl_canopen *co, uint8_t sub, uint32_t val) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t node_id = (uint8_t)(val >> 16);

  if ((val >> 24) != 0U) {
    return PBL_CANOPEN__ABORT_VALUE;
  }
  if ((val & 0xffffU) == 0U || node_id == 0U || node_id > 127U) {
    return 0U;
  }

  for (uint8_t i = 1U; i <= od->num_hb_consumers; i++) {
    uint32_t other = pbl_canopen__od_get_u32(od, PBL_CANOPEN__IDX_HB_CONSUMER, i);

    if (i != sub && (other & 0xffffU) != 0U && (uint8_t)(other >> 16) == node_id) {
      return PBL_CANOPEN__ABORT_PARAM_INCOMPAT;
    }
  }

  return 0U;
}

void pbl_canopen__hb_config(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;
  uint32_t period = pbl_canopen__od_get_u32(od, PBL_CANOPEN__IDX_HB_PRODUCER, 0U);

  if (period != 0U) {
    pbl_delayed_work_reschedule(&pbl_canopen__workq, &co->heartbeat, PBL_MSEC(period));
  } else {
    pbl_delayed_work_cancel(&co->heartbeat);
  }

  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    struct pbl_canopen__hb_consumer *c = &od->hb_consumers[i];
    uint32_t val = pbl_canopen__od_get_u32(od, PBL_CANOPEN__IDX_HB_CONSUMER, i + 1U);
    uint8_t node_id = (uint8_t)(val >> 16);
    uint16_t time_ms = (uint16_t)val;

    if (node_id > 127U || node_id == 0U) {
      time_ms = 0U;
    }
    if (node_id == c->node_id && time_ms == c->time_ms) {
      continue;
    }

    pbl_delayed_work_cancel(&c->timeout);
    c->node_id = node_id;
    c->time_ms = time_ms;
    c->state = PBL_CANOPEN_NMT_INITIALIZING;
    c->alive = false;
  }
}

/* NMT */

static void prv_set_nmt(struct pbl_canopen *co, enum pbl_canopen_nmt_state state);

/* Communication error: bus off, a heartbeat or the SYNC lost. */
void pbl_canopen__comm_error(struct pbl_canopen *co) {
  const struct pbl_canopen_od_entry *e =
      pbl_canopen__od_find(co->cfg.od, PBL_CANOPEN__IDX_ERR_BEHAV, 1U);
  uint8_t behavior = e != nullptr ? *(const uint8_t *)e->data : ERR_BEHAV_PRE_OP;

  if (behavior == ERR_BEHAV_PRE_OP && co->nmt == PBL_CANOPEN_NMT_OPERATIONAL) {
    prv_set_nmt(co, PBL_CANOPEN_NMT_PRE_OPERATIONAL);
  } else if (behavior == ERR_BEHAV_STOPPED) {
    prv_set_nmt(co, PBL_CANOPEN_NMT_STOPPED);
  }
}

static void prv_set_nmt(struct pbl_canopen *co, enum pbl_canopen_nmt_state state) {
  const struct pbl_canopen_callbacks *cb = co->cfg.cb;
  [[maybe_unused]] enum pbl_canopen_nmt_state old = co->nmt;

  if (old == state) {
    return;
  }

  PBL_LOG_DBG("NMT state %02x", state);
  co->nmt = state;
  if (state != PBL_CANOPEN_NMT_PRE_OPERATIONAL && state != PBL_CANOPEN_NMT_OPERATIONAL) {
    pbl_canopen__sdo_reset(co);
    pbl_canopen__sync_stop(co);
  }
  pbl_canopen__pdo_nmt(co);
#ifdef CONFIG_CANOPEN_CIA402
  pbl_canopen__cia402_nmt(co, old);
#endif

  if (cb != nullptr && cb->nmt != nullptr) {
    cb->nmt(co, state, co->cfg.arg);
  }
}

static void prv_reset(struct pbl_canopen *co, enum pbl_canopen_reset reset) {
  const struct pbl_canopen_callbacks *cb = co->cfg.cb;
  const struct pbl_canopen_od *od = co->cfg.od;
  uint8_t bootup = 0U;

  co->nmt = PBL_CANOPEN_NMT_INITIALIZING;
  pbl_canopen__sdo_reset(co);
  pbl_canopen__pdo_stop(co);
  pbl_canopen__sync_stop(co);
  pbl_delayed_work_cancel(&co->heartbeat);
  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    pbl_delayed_work_cancel(&od->hb_consumers[i].timeout);
    od->hb_consumers[i].node_id = 0U;
    od->hb_consumers[i].time_ms = 0U;
  }

  pbl_canopen__od_reset(co, reset == PBL_CANOPEN_RESET_NODE);
  pbl_canopen__emcy_reset(co);
#ifdef CONFIG_CANOPEN_CIA402
  pbl_canopen__cia402_reset(co);
#endif
  if (cb != nullptr && cb->reset != nullptr) {
    cb->reset(co, reset, co->cfg.arg);
  }

  pbl_canopen__send(co, PBL_CANOPEN__COB_HEARTBEAT + co->cfg.node_id, &bootup, 1U);
  pbl_canopen__pdo_config_all(co);
  pbl_canopen__hb_config(co);
  prv_set_nmt(co, PBL_CANOPEN_NMT_PRE_OPERATIONAL);
}

static void prv_nmt_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  if (f->len != 2U || (f->data[1] != 0U && f->data[1] != co->cfg.node_id)) {
    return;
  }

  switch (f->data[0]) {
    case NMT_CS_START:
      prv_set_nmt(co, PBL_CANOPEN_NMT_OPERATIONAL);
      break;
    case NMT_CS_STOP:
      prv_set_nmt(co, PBL_CANOPEN_NMT_STOPPED);
      break;
    case NMT_CS_PRE_OP:
      prv_set_nmt(co, PBL_CANOPEN_NMT_PRE_OPERATIONAL);
      break;
    case NMT_CS_RESET_NODE:
      prv_reset(co, PBL_CANOPEN_RESET_NODE);
      break;
    case NMT_CS_RESET_COMM:
      prv_reset(co, PBL_CANOPEN_RESET_COMM);
      break;
    default:
      break;
  }
}

/* Reception */

static void prv_dispatch(struct pbl_canopen *co, const struct pbl_canopen__frame *f) {
  uint32_t sync = pbl_canopen__od_get_u32(co->cfg.od, PBL_CANOPEN__IDX_SYNC, 0U);
  bool ext = (f->flags & PBL_CAN_FRAME_IDE) != 0U;

  if (!ext && f->id == PBL_CANOPEN__COB_NMT) {
    prv_nmt_receive(co, f);
    return;
  }

  if (co->nmt == PBL_CANOPEN_NMT_STOPPED) {
    if (!ext && f->id > PBL_CANOPEN__COB_HEARTBEAT && f->id <= PBL_CANOPEN__COB_HEARTBEAT + 127U) {
      prv_hb_receive(co, f);
    }
    return;
  }

  if (ext == ((sync & PBL_CANOPEN__COB_EXT) != 0U) && f->id == (sync & PBL_CAN_EXT_ID_MASK)) {
    pbl_canopen__sync_receive(co, f);
  } else if (!ext && f->id == PBL_CANOPEN__COB_SDO_RX + co->cfg.node_id) {
    pbl_canopen__sdo_receive(co, f);
  } else if (!ext && f->id > PBL_CANOPEN__COB_HEARTBEAT &&
             f->id <= PBL_CANOPEN__COB_HEARTBEAT + 127U) {
    prv_hb_receive(co, f);
  } else {
    (void)pbl_canopen__pdo_receive(co, f);
  }
}

static void prv_rx(const struct pbl_can *can, const struct pbl_can_frame *frame, void *arg) {
  struct pbl_canopen *co = arg;
  struct pbl_canopen__frame f = {
    .id = frame->id,
    .cycles = pbl_cycle_get_32(),
    .flags = frame->flags,
    .len = frame->len,
  };

  if ((frame->flags & (PBL_CAN_FRAME_FDF | PBL_CAN_FRAME_RTR)) != 0U) {
    return;
  }

  memcpy(f.data, frame->data, frame->len);
  if (pbl_msgq_put(&co->rxq, &f, PBL_NO_WAIT) != 0) {
    co->rx_overrun = true;
  }
  pbl_work_submit(&pbl_canopen__workq, &co->rx_work);
}

static void prv_rx_work(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, rx_work);
  struct pbl_canopen__frame f;

  while (pbl_msgq_get(&co->rxq, &f, PBL_NO_WAIT) == 0) {
    (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
    if (co->started) {
      prv_dispatch(co, &f);
    }
    pbl_mutex_unlock(&co->lock);
  }

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->rx_overrun && co->started) {
    co->rx_overrun = false;
    co->rx_overruns++;
    (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_CAN_OVERRUN, PBL_CANOPEN__ERR_REG_COMM,
                                  nullptr);
  }
  pbl_mutex_unlock(&co->lock);
}

/* Bus state */

static void prv_can_state(const struct pbl_can *can, enum pbl_can_bus_state state,
                          struct pbl_can_err_cnt cnt, void *arg) {
  struct pbl_canopen *co = arg;

  co->bus_state = state;
  pbl_work_submit(&pbl_canopen__workq, &co->can_work);
}

static void prv_can_work(struct pbl_work *work) {
  struct pbl_canopen *co = PBL_CONTAINER_OF(work, struct pbl_canopen, can_work);
  enum pbl_can_bus_state state = co->bus_state;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (!co->started) {
    pbl_mutex_unlock(&co->lock);
    return;
  }

  switch (state) {
    case PBL_CAN_STATE_ERROR_PASSIVE:
      (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_CAN_PASSIVE, PBL_CANOPEN__ERR_REG_COMM,
                                    nullptr);
      break;
    case PBL_CAN_STATE_BUS_OFF:
      (void)pbl_canopen__emcy_raise(co, PBL_CANOPEN__EMCY_BUS_OFF, PBL_CANOPEN__ERR_REG_COMM,
                                    nullptr);
      pbl_canopen__comm_error(co);
      break;
    case PBL_CAN_STATE_ERROR_ACTIVE:
    case PBL_CAN_STATE_ERROR_WARNING:
      (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_BUS_OFF);
      (void)pbl_canopen__emcy_clear(co, PBL_CANOPEN__EMCY_CAN_PASSIVE);
      break;
    default:
      break;
  }

  prv_tx_kick(co);
  pbl_mutex_unlock(&co->lock);
}

/* API */

static void prv_thread_start(void) {
  (void)pbl_mutex_lock(&s_thread_lock, PBL_FOREVER);
  if (!s_thread_started) {
    const struct pbl_thread_attr attr = {
      .name = "CANopen",
      .entry = pbl_workq_thread,
      .arg = &pbl_canopen__workq,
      .prio = CONFIG_CANOPEN_THREAD_PRIO,
      .privileged = true,
      .stack = s_stack,
      .stack_size = sizeof(s_stack),
    };

    pbl_canopen__workq.thread = &s_thread;
    (void)pbl_thread_create(&s_thread, &attr);
    s_thread_started = true;
  }
  pbl_mutex_unlock(&s_thread_lock);
}

int pbl_canopen_init(struct pbl_canopen *co, const struct pbl_canopen_config *cfg) {
  const struct pbl_canopen_od *od = cfg->od;

  if (cfg->can == nullptr || od == nullptr || cfg->node_id == 0U || cfg->node_id > 127U ||
      cfg->bitrate == 0U) {
    return -EINVAL;
  }

  memset(co, 0, sizeof(*co));
  pbl_mutex_init(&co->lock);
  co->cfg = *cfg;
  co->nmt = PBL_CANOPEN_NMT_INITIALIZING;
  pbl_msgq_init(&co->rxq, co->rx_buf, sizeof(co->rx_buf[0]), PBL_ARRAY_SIZE(co->rx_buf));
  pbl_work_init(&co->rx_work, prv_rx_work);
  pbl_work_init(&co->tx_work, prv_tx_work);
  pbl_work_init(&co->can_work, prv_can_work);
  pbl_delayed_work_init(&co->heartbeat, prv_hb_work);

  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    struct pbl_canopen__hb_consumer *c = &od->hb_consumers[i];

    memset(c, 0, sizeof(*c));
    c->co = co;
    pbl_delayed_work_init(&c->timeout, prv_hb_timeout);
  }

  pbl_canopen__sdo_init(co);
  pbl_canopen__pdo_init(co);
  pbl_canopen__emcy_init(co);

  prv_thread_start();

  return 0;
}

int pbl_canopen_start(struct pbl_canopen *co) {
  const struct pbl_can_config can_cfg = {.bitrate = co->cfg.bitrate};
  int ret;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (co->started) {
    pbl_mutex_unlock(&co->lock);
    return -EALREADY;
  }

  ret = pbl_can_configure(co->cfg.can, &can_cfg);
  if (ret != 0) {
    pbl_mutex_unlock(&co->lock);
    return ret;
  }

  pbl_can_set_rx_callback(co->cfg.can, prv_rx, co);
  pbl_can_set_state_callback(co->cfg.can, prv_can_state, co);
  ret = pbl_can_start(co->cfg.can);
  if (ret != 0) {
    pbl_can_set_rx_callback(co->cfg.can, nullptr, nullptr);
    pbl_can_set_state_callback(co->cfg.can, nullptr, nullptr);
    pbl_mutex_unlock(&co->lock);
    return ret;
  }

  co->started = true;
  co->bus_state = PBL_CAN_STATE_ERROR_ACTIVE;
  prv_reset(co, PBL_CANOPEN_RESET_NODE);
  pbl_mutex_unlock(&co->lock);

  PBL_LOG_DBG("node %u started", co->cfg.node_id);

  return 0;
}

int pbl_canopen_stop(struct pbl_canopen *co) {
  const struct pbl_canopen_od *od = co->cfg.od;
  int ret;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  if (!co->started) {
    pbl_mutex_unlock(&co->lock);
    return 0;
  }
  co->started = false;
  co->nmt = PBL_CANOPEN_NMT_INITIALIZING;
  co->tx_count = 0U;
  pbl_mutex_unlock(&co->lock);

  pbl_can_set_rx_callback(co->cfg.can, nullptr, nullptr);
  pbl_can_set_state_callback(co->cfg.can, nullptr, nullptr);
  ret = pbl_can_stop(co->cfg.can);

  (void)pbl_work_cancel_sync(&co->rx_work);
  (void)pbl_work_cancel_sync(&co->tx_work);
  (void)pbl_work_cancel_sync(&co->can_work);
  (void)pbl_delayed_work_cancel_sync(&co->heartbeat);
  (void)pbl_delayed_work_cancel_sync(&co->sdo.timeout);
  (void)pbl_delayed_work_cancel_sync(&co->emcy_inhibit);
  (void)pbl_delayed_work_cancel_sync(&co->sync_timeout);
  for (uint8_t i = 0U; i < od->num_hb_consumers; i++) {
    (void)pbl_delayed_work_cancel_sync(&od->hb_consumers[i].timeout);
  }
  for (uint16_t i = 0U; i < od->num_rpdos; i++) {
    (void)pbl_delayed_work_cancel_sync(&od->rpdos[i].pdo.timer);
  }
  for (uint16_t i = 0U; i < od->num_tpdos; i++) {
    (void)pbl_delayed_work_cancel_sync(&od->tpdos[i].pdo.timer);
    (void)pbl_delayed_work_cancel_sync(&od->tpdos[i].inhibit);
  }
  pbl_msgq_purge(&co->rxq);

  PBL_LOG_DBG("node %u stopped", co->cfg.node_id);

  return ret;
}

enum pbl_canopen_nmt_state pbl_canopen_nmt_state(struct pbl_canopen *co) {
  enum pbl_canopen_nmt_state state;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  state = co->nmt;
  pbl_mutex_unlock(&co->lock);

  return state;
}

int pbl_canopen_od_read(struct pbl_canopen *co, uint16_t index, uint8_t subindex, void *data,
                        size_t len) {
  const struct pbl_canopen_od_entry *e = pbl_canopen__od_find(co->cfg.od, index, subindex);
  size_t size;

  if (e == nullptr) {
    return -ENOENT;
  }

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  size = pbl_canopen__od_len(e);
  if (len >= size) {
    memcpy(data, e->data, size);
  }
  pbl_mutex_unlock(&co->lock);

  return len >= size ? (int)size : -ENOSPC;
}

int pbl_canopen_od_write(struct pbl_canopen *co, uint16_t index, uint8_t subindex, const void *data,
                         size_t len) {
  const struct pbl_canopen_od_entry *e = pbl_canopen__od_find(co->cfg.od, index, subindex);

  if (e == nullptr) {
    return -ENOENT;
  }
  if (len > e->size || (len < e->size && e->type != PBL_CANOPEN_TYPE_VISIBLE_STRING)) {
    return -EINVAL;
  }

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  pbl_canopen__od_store(co, e, data, len);
  pbl_mutex_unlock(&co->lock);

  return 0;
}

int pbl_canopen_tpdo_request(struct pbl_canopen *co, uint16_t num) {
  int ret;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  ret = pbl_canopen__tpdo_request(co, num);
  pbl_mutex_unlock(&co->lock);

  return ret;
}

int pbl_canopen_emcy_raise(struct pbl_canopen *co, uint16_t code, uint8_t reg,
                           const uint8_t *msef) {
  int ret;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  ret = pbl_canopen__emcy_raise(co, code, reg, msef);
  pbl_mutex_unlock(&co->lock);

  return ret;
}

int pbl_canopen_emcy_clear(struct pbl_canopen *co, uint16_t code) {
  int ret;

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  ret = pbl_canopen__emcy_clear(co, code);
  pbl_mutex_unlock(&co->lock);

  return ret;
}

#if UNITTEST
void pbl_canopen_reset_for_test(void) {
  memset(&s_thread, 0, sizeof(s_thread));
  pbl_workq_init(&pbl_canopen__workq);
  s_thread_started = false;
}
#endif
