/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/ppg.h>
#include <pbl/util/misc.h>

#define NANO_PER_MICRO 1000
#define PICO_PER_NANO  1000

PBL_DEVICE_CLASS_DEFINE(pbl_ppg_class, "ppg");

int pbl_ppg_dev_init(const struct pbl_device *dev) {
  const struct pbl_ppg *ppg = container_of(dev, const struct pbl_ppg, dev);
  int ret;

  pbl_ppg_lock(ppg);
  ret = ppg->ops->init(ppg);
  pbl_ppg_unlock(ppg);

  return ret;
}

void pbl_ppg_lock(const struct pbl_ppg *ppg) {
  pbl_mutex_lock(&ppg->state->lock, PBL_FOREVER);
}

void pbl_ppg_unlock(const struct pbl_ppg *ppg) {
  pbl_mutex_unlock(&ppg->state->lock);
}

int pbl_ppg_set_listener(const struct pbl_ppg *ppg, const struct pbl_ppg_listener *listener) {
  pbl_ppg_lock(ppg);
  ppg->state->listener = listener;
  pbl_ppg_unlock(ppg);

  return 0;
}

const struct pbl_ppg_info *pbl_ppg_get_info(const struct pbl_ppg *ppg) {
  if (!pbl_device_is_ready(&ppg->dev)) {
    return nullptr;
  }

  return ppg->ops->get_info(ppg);
}

static int prv_check_slot(const struct pbl_ppg *ppg, uint8_t slot) {
  if (slot >= ppg->ops->get_info(ppg)->num_slots) {
    return -EINVAL;
  }

  return 0;
}

static int prv_configure_locked(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config) {
  const struct pbl_ppg_info *info = ppg->ops->get_info(ppg);
  bool enabled = false;

  if (ppg->state->running) {
    return -EBUSY;
  }

  if ((config->sample_rate_hz == 0U) || (config->sample_rate_hz > info->max_sample_rate_hz) ||
      (config->batch == 0U)) {
    return -EINVAL;
  }

  for (uint8_t slot = 0U; slot < PBL_PPG_MAX_SLOTS; slot++) {
    if (config->rx_mask[slot] == 0U) {
      continue;
    }

    if ((slot >= info->num_slots) || ((config->rx_mask[slot] & ~info->slots[slot].rx_mask) != 0U)) {
      return -EINVAL;
    }

    enabled = true;
  }

  if (!enabled) {
    return -EINVAL;
  }

  return ppg->ops->configure(ppg, config);
}

int pbl_ppg_configure(const struct pbl_ppg *ppg, const struct pbl_ppg_config *config) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_configure_locked(ppg, config);
  pbl_ppg_unlock(ppg);

  return ret;
}

static int prv_start_locked(const struct pbl_ppg *ppg) {
  int ret;

  if (ppg->state->running) {
    return -EALREADY;
  }

  ret = ppg->ops->start(ppg);
  if (ret != 0) {
    return ret;
  }

  ppg->state->running = true;

  return 0;
}

int pbl_ppg_start(const struct pbl_ppg *ppg) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_start_locked(ppg);
  pbl_ppg_unlock(ppg);

  return ret;
}

static int prv_stop_locked(const struct pbl_ppg *ppg) {
  int ret;

  if (!ppg->state->running) {
    return 0;
  }

  ret = ppg->ops->stop(ppg);
  if (ret != 0) {
    return ret;
  }

  ppg->state->running = false;

  return 0;
}

int pbl_ppg_stop(const struct pbl_ppg *ppg) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_stop_locked(ppg);
  pbl_ppg_unlock(ppg);

  return ret;
}

bool pbl_ppg_is_running(const struct pbl_ppg *ppg) {
  bool running;

  pbl_ppg_lock(ppg);
  running = ppg->state->running;
  pbl_ppg_unlock(ppg);

  return running;
}

static int prv_set_led_current_locked(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led,
                                      uint32_t ua) {
  const struct pbl_ppg_info *info = ppg->ops->get_info(ppg);
  int ret;

  ret = prv_check_slot(ppg, slot);
  if (ret != 0) {
    return ret;
  }

  if ((led >= PBL_PPG_MAX_LED_DRIVERS) || ((info->slots[slot].led_mask & (1U << led)) == 0U) ||
      (ua > info->max_led_current_ua)) {
    return -EINVAL;
  }

  return ppg->ops->set_led_current(ppg, slot, led, ua);
}

int pbl_ppg_set_led_current(const struct pbl_ppg *ppg, uint8_t slot, uint8_t led, uint32_t ua) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_set_led_current_locked(ppg, slot, led, ua);
  pbl_ppg_unlock(ppg);

  return ret;
}

static int prv_check_rx(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx) {
  const struct pbl_ppg_info *info = ppg->ops->get_info(ppg);
  int ret;

  ret = prv_check_slot(ppg, slot);
  if (ret != 0) {
    return ret;
  }

  if ((rx >= info->num_rx) || ((info->slots[slot].rx_mask & (1U << rx)) == 0U)) {
    return -EINVAL;
  }

  return 0;
}

static int prv_set_gain_locked(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm) {
  const struct pbl_ppg_info *info = ppg->ops->get_info(ppg);
  int ret;

  ret = prv_check_rx(ppg, slot, rx);
  if (ret != 0) {
    return ret;
  }

  for (uint8_t i = 0U; i < info->num_gains; i++) {
    if (info->gains_ohm[i] == ohm) {
      return ppg->ops->set_gain(ppg, slot, rx, ohm);
    }
  }

  return -EINVAL;
}

int pbl_ppg_set_gain(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx, uint32_t ohm) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_set_gain_locked(ppg, slot, rx, ohm);
  pbl_ppg_unlock(ppg);

  return ret;
}

static int prv_get_channel_locked(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                                  struct pbl_ppg_channel *channel) {
  int ret;

  ret = prv_check_rx(ppg, slot, rx);
  if (ret != 0) {
    return ret;
  }

  *channel = (struct pbl_ppg_channel){};

  return ppg->ops->get_channel(ppg, slot, rx, channel);
}

int pbl_ppg_get_channel(const struct pbl_ppg *ppg, uint8_t slot, uint8_t rx,
                        struct pbl_ppg_channel *channel) {
  int ret;

  if (!pbl_device_is_ready(&ppg->dev)) {
    return -ENODEV;
  }

  pbl_ppg_lock(ppg);
  ret = prv_get_channel_locked(ppg, slot, rx, channel);
  pbl_ppg_unlock(ppg);

  return ret;
}

int64_t pbl_ppg_value_to_pa(const struct pbl_ppg_info *info, const struct pbl_ppg_channel *channel,
                            int32_t value) {
  int64_t nv;

  if ((info->full_scale == 0) || (channel->calibrated_gain_ohm == 0U)) {
    return 0;
  }

  nv = ((int64_t)value * info->full_scale_uv * NANO_PER_MICRO) / info->full_scale;

  return (nv / channel->calibrated_gain_ohm) * PICO_PER_NANO +
         ((nv % channel->calibrated_gain_ohm) * PICO_PER_NANO) / channel->calibrated_gain_ohm;
}

void pbl_ppg_report_samples(const struct pbl_ppg *ppg, const struct pbl_ppg_batch *batch) {
  const struct pbl_ppg_listener *listener = ppg->state->listener;

  if ((listener == nullptr) || (listener->samples == nullptr)) {
    return;
  }

  listener->samples(ppg, batch, listener->user_data);
}
