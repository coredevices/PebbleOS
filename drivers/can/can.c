/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/drivers/can.h>
#include <pbl/kernel/irq.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_can_class, "can");

#define SYNC_SEG 1U

#define DATA_SAMPLE_POINT 750U

static const uint8_t s_dlc_len[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

struct prv_sync {
  struct pbl_sem done;
  int status;
};

static const struct pbl_can *prv_can(const struct pbl_device *dev) {
  return PBL_CONTAINER_OF(dev, const struct pbl_can, dev);
}

int pbl_can_dev_init(const struct pbl_device *dev) {
  const struct pbl_can *can = prv_can(dev);

  return can->ops->init(can);
}

void pbl_can_receive(const struct pbl_can *can, const struct pbl_can_frame *frame) {
  struct pbl_can_state *state = can->state;

  if (state->rx_cb != nullptr) {
    state->rx_cb(can, frame, state->rx_arg);
  }
}

void pbl_can_tx_done(const struct pbl_can *can, pbl_can_tx_callback_t cb, void *arg, int status) {
  pbl_sem_give(&can->state->tx_slots);

  if (cb != nullptr) {
    cb(can, status, arg);
  }
}

void pbl_can_report_state(const struct pbl_can *can, enum pbl_can_bus_state state,
                          struct pbl_can_err_cnt cnt) {
  struct pbl_can_state *s = can->state;

  if (state == PBL_CAN_STATE_BUS_OFF && s->bus_state != PBL_CAN_STATE_BUS_OFF) {
    s->stats.bus_off++;
  }

  s->bus_state = state;
  if (s->state_cb != nullptr) {
    s->state_cb(can, state, cnt, s->state_arg);
  }
}

void pbl_can_report_error(const struct pbl_can *can, enum pbl_can_error error) {
  if (error < PBL_CAN_ERROR_COUNT) {
    can->state->stats.errors[error]++;
  }
}

uint8_t pbl_can_dlc_to_len(uint8_t dlc) {
  return s_dlc_len[dlc & 0xfU];
}

uint8_t pbl_can_len_to_dlc(uint8_t len) {
  uint8_t dlc = 0U;

  while (dlc < 15U && s_dlc_len[dlc] < len) {
    dlc++;
  }

  return dlc;
}

// CiA 301 recommends 87.5%, lower at the highest bit rates
static uint16_t prv_default_sample_point(uint32_t bitrate) {
  if (bitrate > 800000U) {
    return 750U;
  }
  if (bitrate > 500000U) {
    return 800U;
  }
  return 875U;
}

int pbl_can_calc_timing(const struct pbl_can_timing_limits *limits, uint32_t clock,
                        uint32_t bitrate, uint16_t sample_point, struct pbl_can_timing *timing) {
  const uint32_t total_min = SYNC_SEG + limits->tseg1_min + limits->tseg2_min;
  const uint32_t total_max = SYNC_SEG + limits->tseg1_max + limits->tseg2_max;
  uint32_t best_err = UINT32_MAX;

  if (bitrate == 0U || sample_point == 0U || sample_point >= 1000U) {
    return -EINVAL;
  }

  for (uint32_t psc = limits->prescaler_min; psc <= limits->prescaler_max; psc++) {
    uint64_t div = (uint64_t)psc * bitrate;
    uint32_t total, tseg1, tseg2, sp, err;

    if (div > clock) {
      break;
    }
    if (clock % div != 0U) {
      continue;
    }

    total = (uint32_t)(clock / div);
    if (total < total_min) {
      break;
    }
    if (total > total_max) {
      continue;
    }

    tseg2 = total - PBL_DIV_ROUND_CLOSEST(total * sample_point, 1000U);
    tseg2 = PBL_CLAMP(tseg2, limits->tseg2_min, limits->tseg2_max);
    tseg1 = total - SYNC_SEG - tseg2;
    if (tseg1 > limits->tseg1_max) {
      tseg1 = limits->tseg1_max;
    } else if (tseg1 < limits->tseg1_min) {
      tseg1 = limits->tseg1_min;
    }
    tseg2 = total - SYNC_SEG - tseg1;
    if (tseg2 < limits->tseg2_min || tseg2 > limits->tseg2_max) {
      continue;
    }

    sp = (SYNC_SEG + tseg1) * 1000U / total;
    err = (sp > sample_point) ? sp - sample_point : sample_point - sp;
    if (err < best_err) {
      best_err = err;
      timing->prescaler = (uint16_t)psc;
      timing->tseg1 = (uint16_t)tseg1;
      timing->tseg2 = (uint16_t)tseg2;
      timing->sjw = (uint16_t)PBL_MIN(PBL_MIN(tseg1, tseg2), limits->sjw_max);
      if (err == 0U) {
        break;
      }
    }
  }

  return (best_err == UINT32_MAX) ? -EINVAL : 0;
}

int pbl_can_configure(const struct pbl_can *can, const struct pbl_can_config *cfg) {
  struct pbl_can_state *state = can->state;
  struct pbl_can_timing nominal, data;
  const bool fd = (cfg->mode & PBL_CAN_MODE_FD) != 0U;
  uint32_t clock;
  int ret;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  if ((cfg->mode & ~can->modes) != 0U) {
    return -ENOTSUP;
  }

  if ((cfg->mode & PBL_CAN_MODE_FD_NON_ISO) != 0U && !fd) {
    return -EINVAL;
  }

  ret = can->ops->get_core_clock(can, &clock);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_can_calc_timing(
      can->limits, clock, cfg->bitrate,
      (cfg->sample_point != 0U) ? cfg->sample_point : prv_default_sample_point(cfg->bitrate),
      &nominal);
  if (ret != 0) {
    return ret;
  }

  if (fd) {
    ret = pbl_can_calc_timing(
        can->data_limits, clock, cfg->data_bitrate,
        (cfg->data_sample_point != 0U) ? cfg->data_sample_point : DATA_SAMPLE_POINT, &data);
    if (ret != 0) {
      return ret;
    }
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->started) {
    ret = -EBUSY;
    goto out;
  }

  ret = can->ops->configure(can, &nominal, fd ? &data : nullptr, cfg->mode);
  if (ret != 0) {
    goto out;
  }

  state->cfg = *cfg;
  if (!fd) {
    state->cfg.data_bitrate = 0U;
  }
  state->configured = true;

out:
  pbl_mutex_unlock(&state->lock);

  return ret;
}

int pbl_can_config_get(const struct pbl_can *can, struct pbl_can_config *cfg) {
  struct pbl_can_state *state = can->state;
  int ret = 0;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);
  if (state->configured) {
    *cfg = state->cfg;
  } else {
    ret = -EINVAL;
  }
  pbl_mutex_unlock(&state->lock);

  return ret;
}

static bool prv_filter_valid(const struct pbl_can_filter *filter) {
  const uint32_t max =
      ((filter->flags & PBL_CAN_FRAME_IDE) != 0U) ? PBL_CAN_EXT_ID_MASK : PBL_CAN_STD_ID_MASK;

  return (filter->flags & ~PBL_CAN_FRAME_IDE) == 0U && filter->id <= max && filter->mask <= max;
}

int pbl_can_add_filter(const struct pbl_can *can, const struct pbl_can_filter *filter) {
  struct pbl_can_state *state = can->state;
  int ret = -ENOSPC;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  if (!prv_filter_valid(filter)) {
    return -EINVAL;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->started) {
    ret = -EBUSY;
    goto out;
  }

  for (uint8_t i = 0U; i < can->num_filters; i++) {
    if ((state->filters & PBL_BIT(i)) == 0U) {
      ret = can->ops->set_filter(can, i, filter);
      if (ret == 0) {
        state->filters |= PBL_BIT(i);
        ret = i;
      }
      break;
    }
  }

out:
  pbl_mutex_unlock(&state->lock);

  return ret;
}

int pbl_can_remove_filter(const struct pbl_can *can, int idx) {
  struct pbl_can_state *state = can->state;
  int ret;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->started) {
    ret = -EBUSY;
    goto out;
  }

  if (idx < 0 || idx >= can->num_filters || (state->filters & PBL_BIT(idx)) == 0U) {
    ret = -EINVAL;
    goto out;
  }

  ret = can->ops->set_filter(can, (uint8_t)idx, nullptr);
  if (ret == 0) {
    state->filters &= ~PBL_BIT(idx);
  }

out:
  pbl_mutex_unlock(&state->lock);

  return ret;
}

void pbl_can_set_rx_callback(const struct pbl_can *can, pbl_can_rx_callback_t cb, void *arg) {
  struct pbl_can_state *state = can->state;

  pbl_irq_lock();
  state->rx_cb = cb;
  state->rx_arg = arg;
  pbl_irq_unlock();
}

void pbl_can_set_state_callback(const struct pbl_can *can, pbl_can_state_callback_t cb, void *arg) {
  struct pbl_can_state *state = can->state;

  pbl_irq_lock();
  state->state_cb = cb;
  state->state_arg = arg;
  pbl_irq_unlock();
}

int pbl_can_start(const struct pbl_can *can) {
  struct pbl_can_state *state = can->state;
  int ret = 0;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->started) {
    goto out;
  }

  if (!state->configured) {
    ret = -EINVAL;
    goto out;
  }

  state->bus_state = PBL_CAN_STATE_ERROR_ACTIVE;
  ret = can->ops->start(can);
  if (ret != 0) {
    state->bus_state = PBL_CAN_STATE_STOPPED;
    goto out;
  }

  pbl_irq_lock();
  state->started = true;
  pbl_irq_unlock();

out:
  pbl_mutex_unlock(&state->lock);

  return ret;
}

int pbl_can_stop(const struct pbl_can *can) {
  struct pbl_can_state *state = can->state;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (state->started) {
    pbl_irq_lock();
    state->started = false;
    can->ops->stop(can);
    state->bus_state = PBL_CAN_STATE_STOPPED;
    pbl_irq_unlock();
  }

  pbl_mutex_unlock(&state->lock);

  return 0;
}

static bool prv_frame_valid(const struct pbl_can *can, const struct pbl_can_frame *frame) {
  const uint32_t max =
      ((frame->flags & PBL_CAN_FRAME_IDE) != 0U) ? PBL_CAN_EXT_ID_MASK : PBL_CAN_STD_ID_MASK;

  if (frame->id > max || (frame->flags & PBL_CAN_FRAME_ESI) != 0U) {
    return false;
  }

  if ((frame->flags & PBL_CAN_FRAME_FDF) != 0U) {
    return (can->state->cfg.mode & PBL_CAN_MODE_FD) != 0U &&
           (frame->flags & PBL_CAN_FRAME_RTR) == 0U && frame->len <= PBL_CAN_FD_MAX_LEN &&
           pbl_can_dlc_to_len(pbl_can_len_to_dlc(frame->len)) == frame->len;
  }

  return (frame->flags & PBL_CAN_FRAME_BRS) == 0U && frame->len <= PBL_CAN_MAX_LEN;
}

static void prv_sync_done(const struct pbl_can *can, int status, void *arg) {
  struct prv_sync *sync = arg;

  sync->status = status;
  pbl_sem_give(&sync->done);
}

int pbl_can_send(const struct pbl_can *can, const struct pbl_can_frame *frame,
                 pbl_timeout_t timeout, pbl_can_tx_callback_t cb, void *arg) {
  struct pbl_can_state *state = can->state;
  struct prv_sync sync;
  int ret;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  if (cb == nullptr && pbl_in_isr()) {
    return -EINVAL;
  }

  if (!state->started) {
    return -ENETDOWN;
  }

  if ((state->cfg.mode & PBL_CAN_MODE_LISTEN_ONLY) != 0U) {
    return -EPERM;
  }

  if (!prv_frame_valid(can, frame)) {
    return -EINVAL;
  }

  if (pbl_sem_take(&state->tx_slots, timeout) != 0) {
    return -EAGAIN;
  }

  if (cb == nullptr) {
    pbl_sem_init(&sync.done, 0U, 1U);
  }

  pbl_irq_lock();
  if (state->started) {
    ret = can->ops->send(can, frame, (cb != nullptr) ? cb : prv_sync_done,
                         (cb != nullptr) ? arg : &sync);
  } else {
    ret = -ENETDOWN;
  }
  pbl_irq_unlock();

  if (ret != 0) {
    pbl_sem_give(&state->tx_slots);
    goto out;
  }

  if (cb != nullptr) {
    return 0;
  }

  if (pbl_sem_take(&sync.done, timeout) != 0) {
    pbl_irq_lock();
    can->ops->abort(can);
    pbl_irq_unlock();
    (void)pbl_sem_take(&sync.done, PBL_FOREVER);
    if (sync.status == -ECANCELED) {
      sync.status = -ETIMEDOUT;
    }
  }

  ret = sync.status;

out:
  if (cb == nullptr) {
    pbl_sem_deinit(&sync.done);
  }

  return ret;
}

int pbl_can_get_state(const struct pbl_can *can, enum pbl_can_bus_state *state,
                      struct pbl_can_err_cnt *cnt) {
  struct pbl_can_err_cnt local = {0};
  int ret = 0;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  pbl_irq_lock();
  if (can->state->started) {
    ret = can->ops->get_state(can, state, &local);
  } else {
    *state = PBL_CAN_STATE_STOPPED;
  }
  pbl_irq_unlock();

  if (cnt != nullptr) {
    *cnt = local;
  }

  return ret;
}

int pbl_can_recover(const struct pbl_can *can) {
  struct pbl_can_state *state = can->state;
  int ret = 0;

  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&state->lock, PBL_FOREVER);

  if (!state->started) {
    ret = -ENETDOWN;
  } else if ((state->cfg.mode & PBL_CAN_MODE_MANUAL_RECOVERY) == 0U) {
    ret = -ENOTSUP;
  } else {
    pbl_irq_lock();
    if (state->bus_state == PBL_CAN_STATE_BUS_OFF) {
      ret = can->ops->recover(can);
    }
    pbl_irq_unlock();
  }

  pbl_mutex_unlock(&state->lock);

  return ret;
}

int pbl_can_get_stats(const struct pbl_can *can, struct pbl_can_stats *stats) {
  if (!pbl_device_is_ready(&can->dev)) {
    return -ENODEV;
  }

  pbl_irq_lock();
  *stats = can->state->stats;
  pbl_irq_unlock();

  return 0;
}
