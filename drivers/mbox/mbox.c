/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/mbox.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/types.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_mbox_class, "mbox");

static void prv_poll(struct pbl_timer *timer);

static int prv_xlate(const struct pbl_mbox_chan *chan) {
  int idx = chan->ctrl->ops->xlate(chan->ctrl, chan);

  if (idx < 0) {
    return idx;
  }

  if (idx >= chan->ctrl->num_chans) {
    return -EINVAL;
  }

  return idx;
}

static void prv_poll_start(const struct pbl_mbox_controller *ctrl) {
  struct pbl_timer *timer = &ctrl->state->poll;

  if (ctrl->txpoll_period_ms != 0U && !pbl_timer_is_active(timer)) {
    pbl_timer_start(timer, PBL_MSEC(ctrl->txpoll_period_ms));
  }
}

static int prv_send_locked(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg) {
  struct pbl_mbox_chan_state *st = &ctrl->chans[idx];
  int ret;

  ret = ctrl->ops->send_data(ctrl, idx, msg);
  if (ret != 0) {
    return ret;
  }

  st->tx_msg = msg;
  st->tx_active = true;
  st->tx_count++;
  prv_poll_start(ctrl);

  return 0;
}

static void prv_txdone(const struct pbl_mbox_controller *ctrl, uint8_t idx, int ret) {
  struct pbl_mbox_chan_state *st = &ctrl->chans[idx];
  const struct pbl_mbox_client *cl;
  const void *msg;

  pbl_irq_lock();
  if (!st->tx_active) {
    pbl_irq_unlock();
    return;
  }

  st->tx_active = false;
  msg = st->tx_msg;
  cl = st->cl;
  if (st->tx_queued) {
    st->tx_queued = false;
    (void)prv_send_locked(ctrl, idx, st->tx_next);
  }
  pbl_irq_unlock();

  if (cl != nullptr && cl->tx_done != nullptr) {
    cl->tx_done(cl, msg, ret);
  }
}

static void prv_poll(struct pbl_timer *timer) {
  struct pbl_mbox_controller_state *cst =
      container_of(timer, struct pbl_mbox_controller_state, poll);
  const struct pbl_mbox_controller *ctrl = cst->ctrl;
  bool pending = false;

  for (uint8_t idx = 0U; idx < ctrl->num_chans; idx++) {
    if (!ctrl->chans[idx].tx_active) {
      continue;
    }

    if (ctrl->ops->last_tx_done(ctrl, idx)) {
      prv_txdone(ctrl, idx, 0);
    }

    pending |= ctrl->chans[idx].tx_active;
  }

  if (pending) {
    prv_poll_start(ctrl);
  }
}

int pbl_mbox_request(const struct pbl_mbox_chan *chan, const struct pbl_mbox_client *cl) {
  const struct pbl_mbox_controller *ctrl = chan->ctrl;
  struct pbl_mbox_chan_state *st;
  int idx;
  int ret;

  if (!pbl_device_is_ready(&ctrl->dev)) {
    return -ENODEV;
  }

  idx = prv_xlate(chan);
  if (idx < 0) {
    return idx;
  }

  st = &ctrl->chans[idx];

  pbl_irq_lock();
  if (st->cl != nullptr) {
    pbl_irq_unlock();
    return -EBUSY;
  }
  if (ctrl->state->ctrl == nullptr) {
    ctrl->state->ctrl = ctrl;
    pbl_timer_init(&ctrl->state->poll, prv_poll);
  }
  st->cl = cl;
  st->tx_active = false;
  st->tx_queued = false;
  pbl_irq_unlock();

  ret = ctrl->ops->startup(ctrl, (uint8_t)idx);
  if (ret != 0) {
    st->cl = nullptr;
    return ret;
  }

  return 0;
}

void pbl_mbox_free(const struct pbl_mbox_chan *chan) {
  const struct pbl_mbox_controller *ctrl = chan->ctrl;
  struct pbl_mbox_chan_state *st;
  int idx = prv_xlate(chan);

  if (idx < 0) {
    return;
  }

  st = &ctrl->chans[idx];
  ctrl->ops->shutdown(ctrl, (uint8_t)idx);

  pbl_irq_lock();
  st->tx_active = false;
  st->tx_queued = false;
  st->cl = nullptr;
  pbl_irq_unlock();
}

int pbl_mbox_send(const struct pbl_mbox_chan *chan, const void *msg) {
  const struct pbl_mbox_controller *ctrl = chan->ctrl;
  struct pbl_mbox_chan_state *st;
  int idx = prv_xlate(chan);
  int ret = 0;

  if (idx < 0) {
    return idx;
  }

  st = &ctrl->chans[idx];

  pbl_irq_lock();
  if (st->cl == nullptr) {
    ret = -EINVAL;
  } else if (st->tx_active) {
    st->tx_next = msg;
    st->tx_queued = true;
  } else {
    ret = prv_send_locked(ctrl, (uint8_t)idx, msg);
  }
  pbl_irq_unlock();

  return ret;
}

void pbl_mbox_chan_received(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg) {
  struct pbl_mbox_chan_state *st;

  if (idx >= ctrl->num_chans) {
    return;
  }

  st = &ctrl->chans[idx];
  st->rx_count++;

  if (st->cl != nullptr && st->cl->rx_callback != nullptr) {
    st->cl->rx_callback(st->cl, msg);
  }
}

void pbl_mbox_chan_txdone(const struct pbl_mbox_controller *ctrl, uint8_t idx, int ret) {
  if (idx >= ctrl->num_chans) {
    return;
  }

  prv_txdone(ctrl, idx, ret);
}
