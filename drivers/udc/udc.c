/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/udc.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_udc_class, "udc");

static const struct pbl_udc *prv_udc(const struct pbl_device *dev) {
  return PBL_CONTAINER_OF(dev, const struct pbl_udc, dev);
}

#ifdef CONFIG_UDC_VBUS_CHARGER
static void prv_vbus_changed(const struct pbl_charger *charger,
                             struct pbl_charger_listener *listener) {
  struct pbl_udc_state *state = PBL_CONTAINER_OF(listener, struct pbl_udc_state, vbus_listener);
  bool online = false;

  (void)pbl_charger_is_online(charger, &online);
  pbl_udc_vbus_changed(state->udc, online);
}

static int prv_vbus_init(const struct pbl_udc *udc) {
  struct pbl_udc_state *state = udc->state;
  int ret;

  ret = pbl_device_init(&udc->vbus_charger->dev);
  if (ret != 0) {
    return ret;
  }

  state->udc = udc;
  state->vbus_listener.online_changed = prv_vbus_changed;
  pbl_charger_add_listener(udc->vbus_charger, &state->vbus_listener);

  return pbl_charger_is_online(udc->vbus_charger, &state->vbus);
}
#endif

int pbl_udc_dev_init(const struct pbl_device *dev) {
  const struct pbl_udc *udc = prv_udc(dev);
  int ret;

  udc->state->vbus = true;
  ret = udc->ops->init(udc);
  if (ret != 0) {
    return ret;
  }

#ifdef CONFIG_UDC_VBUS_CHARGER
  if (udc->vbus_charger != nullptr) {
    return prv_vbus_init(udc);
  }
#endif

  return 0;
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

void pbl_udc_vbus_changed(const struct pbl_udc *udc, bool present) {
  if (present == udc->state->vbus) {
    return;
  }

  udc->state->vbus = present;
  pbl_udc_event(udc, present ? PBL_UDC_EVENT_VBUS_READY : PBL_UDC_EVENT_VBUS_REMOVED, nullptr);
}

bool pbl_udc_vbus_present(const struct pbl_udc *udc) {
  return udc->state->vbus;
}

int pbl_udc_wakeup(const struct pbl_udc *udc) {
  if (udc->ops->wakeup == nullptr) {
    return -ENOTSUP;
  }

  return udc->ops->wakeup(udc);
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
