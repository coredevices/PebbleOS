/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/charger.h>
#include <pbl/kernel/assert.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_charger, CONFIG_DRIVER_CHARGER_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_charger_class, "charger");

static void prv_lock(const struct pbl_charger *charger) {
  PBL_ASSERTN(pbl_device_is_ready(&charger->dev));
  pbl_mutex_lock(&charger->state->lock, PBL_FOREVER);
}

static void prv_unlock(const struct pbl_charger *charger) {
  pbl_mutex_unlock(&charger->state->lock);
}

int pbl_charger_dev_init(const struct pbl_device *dev) {
  const struct pbl_charger *charger = PBL_CONTAINER_OF(dev, const struct pbl_charger, dev);
  int ret;

  if (charger->ops->init == nullptr) {
    return 0;
  }

  pbl_mutex_lock(&charger->state->lock, PBL_FOREVER);
  ret = charger->ops->init(charger);
  pbl_mutex_unlock(&charger->state->lock);

  return ret;
}

int pbl_charger_is_online(const struct pbl_charger *charger, bool *online) {
  int ret;

  prv_lock(charger);
  ret = charger->ops->is_online(charger, online);
  prv_unlock(charger);

  return ret;
}

int pbl_charger_get_status(const struct pbl_charger *charger, enum pbl_charger_status *status) {
  int ret;

  prv_lock(charger);
  ret = charger->ops->get_status(charger, status);
  prv_unlock(charger);

  return ret;
}

int pbl_charger_is_charging(const struct pbl_charger *charger, bool *charging) {
  enum pbl_charger_status status;
  int ret;

  ret = pbl_charger_get_status(charger, &status);
  if (ret != 0) {
    return ret;
  }

  *charging = status == PBL_CHARGER_STATUS_TRICKLE ||
              status == PBL_CHARGER_STATUS_CONSTANT_CURRENT ||
              status == PBL_CHARGER_STATUS_CONSTANT_VOLTAGE;

  return 0;
}

int pbl_charger_get_measurements(const struct pbl_charger *charger,
                                 struct pbl_charger_measurements *meas) {
  int ret;

  prv_lock(charger);
  ret = charger->ops->get_measurements(charger, meas);
  prv_unlock(charger);

  return ret;
}

int pbl_charger_enable(const struct pbl_charger *charger, bool enable) {
  int ret;

  prv_lock(charger);
  ret = charger->ops->enable(charger, enable);
  prv_unlock(charger);

  return ret;
}

int pbl_charger_high_load_request(const struct pbl_charger *charger) {
  struct pbl_charger_state *state = charger->state;
  int ret = 0;

  if (charger->ops->set_high_load == nullptr) {
    return -ENOTSUP;
  }

  prv_lock(charger);
  if (state->high_load_count == 0U) {
    ret = charger->ops->set_high_load(charger, true);
  }
  if (ret == 0) {
    state->high_load_count++;
  }
  prv_unlock(charger);

  return ret;
}

int pbl_charger_high_load_release(const struct pbl_charger *charger) {
  struct pbl_charger_state *state = charger->state;
  int ret = 0;

  if (charger->ops->set_high_load == nullptr) {
    return -ENOTSUP;
  }

  prv_lock(charger);
  if (state->high_load_count == 0U) {
    prv_unlock(charger);
    return -EALREADY;
  }

  // A failed release keeps the count down, the next request applies the limit again
  state->high_load_count--;
  if (state->high_load_count == 0U) {
    ret = charger->ops->set_high_load(charger, false);
  }
  prv_unlock(charger);

  return ret;
}

void pbl_charger_set_listener(const struct pbl_charger *charger,
                              const struct pbl_charger_listener *listener) {
  prv_lock(charger);
  charger->state->listener = listener;
  prv_unlock(charger);
}

void pbl_charger_report_online_changed(const struct pbl_charger *charger) {
  const struct pbl_charger_listener *listener;

  prv_lock(charger);
  listener = charger->state->listener;
  if (listener != nullptr && listener->online_changed != nullptr) {
    listener->online_changed(charger);
  }
  prv_unlock(charger);
}
