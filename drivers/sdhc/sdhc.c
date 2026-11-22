/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/sdhc.h>
#include <pbl/kernel/types.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_sdhc_class, "sdhc");

int pbl_sdhc_dev_init(const struct pbl_device *dev) {
  const struct pbl_sdhc *sdhc = PBL_CONTAINER_OF(dev, const struct pbl_sdhc, dev);

  return sdhc->ops->init(sdhc);
}

int pbl_sdhc_set_ios(const struct pbl_sdhc *sdhc, const struct pbl_sdhc_ios *ios) {
  int ret;

  if (!pbl_device_is_ready(&sdhc->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&sdhc->state->lock, PBL_FOREVER);
  ret = sdhc->ops->set_ios(sdhc, ios);
  pbl_mutex_unlock(&sdhc->state->lock);

  return ret;
}

int pbl_sdhc_request(const struct pbl_sdhc *sdhc, struct pbl_sdhc_cmd *cmd,
                     const struct pbl_sdhc_data *data) {
  int ret;

  if (!pbl_device_is_ready(&sdhc->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&sdhc->state->lock, PBL_FOREVER);
  ret = sdhc->ops->request(sdhc, cmd, data);
  pbl_mutex_unlock(&sdhc->state->lock);

  return ret;
}

int pbl_sdhc_get_cd(const struct pbl_sdhc *sdhc) {
  if (!pbl_device_is_ready(&sdhc->dev)) {
    return -ENODEV;
  }

  return sdhc->ops->get_cd != nullptr ? sdhc->ops->get_cd(sdhc) : 1;
}
