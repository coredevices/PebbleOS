/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/remoteproc.h>
#include <pbl/kernel/types.h>

PBL_DEVICE_CLASS_DEFINE(pbl_rproc_class, "remoteproc");

int pbl_rproc_boot(const struct pbl_rproc *rproc) {
  struct pbl_rproc_state *st = rproc->state;
  int ret = 0;

  if (!pbl_device_is_ready(&rproc->dev)) {
    return -ENODEV;
  }

  (void)pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (st->power == UINT8_MAX) {
    ret = -EOVERFLOW;
  } else if (st->power == 0U) {
    ret = rproc->ops->start(rproc);
  }

  if (ret == 0) {
    st->power++;
  }
  pbl_mutex_unlock(&st->lock);

  return ret;
}

void pbl_rproc_shutdown(const struct pbl_rproc *rproc) {
  struct pbl_rproc_state *st = rproc->state;

  (void)pbl_mutex_lock(&st->lock, PBL_FOREVER);
  if (st->power > 0U && --st->power == 0U) {
    rproc->ops->stop(rproc);
  }
  pbl_mutex_unlock(&st->lock);
}

bool pbl_rproc_is_running(const struct pbl_rproc *rproc) {
  return rproc->state->power > 0U;
}
