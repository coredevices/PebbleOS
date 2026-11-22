/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/udc.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_udc_class, "udc");

static const struct pbl_udc *prv_udc(const struct pbl_device *dev) {
  return PBL_CONTAINER_OF(dev, const struct pbl_udc, dev);
}

int pbl_udc_dev_init(const struct pbl_device *dev) {
  const struct pbl_udc *udc = prv_udc(dev);

  return udc->ops->init(udc);
}

static bool prv_ep_valid(const struct pbl_udc *udc, uint8_t ep) {
  return (ep & ~PBL_UDC_EP_IN) < udc->num_eps;
}

void pbl_udc_register(const struct pbl_udc *udc, const struct pbl_udc_callbacks *cb, void *arg) {
  udc->state->arg = arg;
  udc->state->cb = cb;
}

int pbl_udc_enable(const struct pbl_udc *udc) {
  if (!pbl_device_is_ready(&udc->dev)) {
    return -ENODEV;
  }

  return udc->ops->enable(udc);
}

int pbl_udc_disable(const struct pbl_udc *udc) {
  return udc->ops->disable(udc);
}

enum pbl_udc_speed pbl_udc_speed(const struct pbl_udc *udc) {
  return udc->ops->speed(udc);
}

int pbl_udc_set_address(const struct pbl_udc *udc, uint8_t addr) {
  return udc->ops->set_address(udc, addr);
}

int pbl_udc_ep_enable(const struct pbl_udc *udc, uint8_t ep, enum pbl_udc_ep_type type,
                      uint16_t mps) {
  if (!prv_ep_valid(udc, ep)) {
    return -EINVAL;
  }

  return udc->ops->ep_enable(udc, ep, type, mps);
}

int pbl_udc_ep_disable(const struct pbl_udc *udc, uint8_t ep) {
  if (!prv_ep_valid(udc, ep)) {
    return -EINVAL;
  }

  return udc->ops->ep_disable(udc, ep);
}

int pbl_udc_ep_set_halt(const struct pbl_udc *udc, uint8_t ep, bool halt) {
  if (!prv_ep_valid(udc, ep)) {
    return -EINVAL;
  }

  return udc->ops->ep_set_halt(udc, ep, halt);
}

int pbl_udc_ep_queue(const struct pbl_udc *udc, uint8_t ep, void *buf, size_t len) {
  if (!prv_ep_valid(udc, ep)) {
    return -EINVAL;
  }

  return udc->ops->ep_queue(udc, ep, buf, len);
}

void pbl_udc_event(const struct pbl_udc *udc, enum pbl_udc_event evt, const uint8_t *setup) {
  const struct pbl_udc_callbacks *cb = udc->state->cb;

  if (cb != nullptr && cb->event != nullptr) {
    cb->event(udc, evt, setup, udc->state->arg);
  }
}

void pbl_udc_xfer_done(const struct pbl_udc *udc, uint8_t ep, int ret) {
  const struct pbl_udc_callbacks *cb = udc->state->cb;

  if (cb != nullptr && cb->xfer_done != nullptr) {
    cb->xfer_done(udc, ep, ret, udc->state->arg);
  }
}
