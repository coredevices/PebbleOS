/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/adc.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_adc_class, "adc");

int pbl_adc_dev_init(const struct pbl_device *dev) {
  const struct pbl_adc *adc = container_of(dev, const struct pbl_adc, dev);
  int ret;

  if (adc->ops->init == nullptr) {
    return 0;
  }

  pbl_mutex_lock(&adc->state->lock, PBL_FOREVER);
  ret = adc->ops->init(adc);
  pbl_mutex_unlock(&adc->state->lock);

  return ret;
}

int pbl_adc_read(const struct pbl_adc_channel *channel, int32_t *microvolt) {
  const struct pbl_adc *adc = channel->adc;
  int ret;

  if (!pbl_device_is_ready(&adc->dev)) {
    return -ENODEV;
  }

  if (channel->id >= adc->num_channels) {
    return -EINVAL;
  }

  pbl_mutex_lock(&adc->state->lock, PBL_FOREVER);
  ret = adc->ops->read(adc, channel->id, microvolt);
  pbl_mutex_unlock(&adc->state->lock);

  return ret;
}
