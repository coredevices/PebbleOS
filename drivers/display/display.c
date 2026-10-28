/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/util/misc.h>

PBL_DEVICE_CLASS_DEFINE(pbl_display_class, "display");

struct prv_sync {
  struct pbl_sem done;
  int status;
};

int pbl_display_init(const struct pbl_device *dev) {
  const struct pbl_display *disp = container_of(dev, const struct pbl_display, dev);
  int ret;

  *disp->state = (struct pbl_display_state){0};

  ret = disp->ops->init(disp);
  if (ret != 0) {
    return ret;
  }

  return disp->ops->set_rotated(disp, disp->rotated);
}

void pbl_display_complete(const struct pbl_display *disp, int status) {
  struct pbl_display_state *state = disp->state;
  pbl_display_callback_t cb = state->cb;
  void *arg = state->arg;

  state->busy = false;
  cb(status, arg);
}

int pbl_display_enable(const struct pbl_display *disp) {
  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  return disp->ops->enable(disp);
}

int pbl_display_disable(const struct pbl_display *disp) {
  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  return disp->ops->disable(disp);
}

int pbl_display_set_rotated(const struct pbl_display *disp, bool rotated) {
  int ret;

  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  ret = disp->ops->set_rotated(disp, rotated != disp->rotated);
  if (ret != 0) {
    return ret;
  }

  disp->state->rotated = rotated;

  return 0;
}

int pbl_display_clear(const struct pbl_display *disp) {
  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  if (disp->ops->clear == nullptr) {
    return -ENOTSUP;
  }

  if (disp->state->busy) {
    return -EBUSY;
  }

  return disp->ops->clear(disp);
}

int pbl_display_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                       uint16_t y0, uint16_t y1, pbl_display_callback_t cb, void *arg) {
  struct pbl_display_state *state = disp->state;
  int ret;

  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  if (y0 > y1 || y1 >= disp->height) {
    return -EINVAL;
  }

  pbl_irq_lock();
  if (state->busy) {
    pbl_irq_unlock();
    return -EBUSY;
  }
  state->busy = true;
  pbl_irq_unlock();

  state->cb = cb;
  state->arg = arg;

  ret = disp->ops->update(disp, buf, y0, y1);
  if (ret != 0) {
    state->busy = false;
  }

  return ret;
}

static void prv_sync_complete(int status, void *arg) {
  struct prv_sync *sync = arg;

  sync->status = status;
  pbl_sem_give(&sync->done);
}

int pbl_display_update_sync(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                            uint16_t y0, uint16_t y1) {
  struct prv_sync sync;
  int ret;

  pbl_sem_init(&sync.done, 0, 1);

  ret = pbl_display_update(disp, buf, y0, y1, prv_sync_complete, &sync);
  if (ret == 0) {
    (void)pbl_sem_take(&sync.done, PBL_FOREVER);
    ret = sync.status;
  }

  pbl_sem_deinit(&sync.done);

  return ret;
}

bool pbl_display_update_in_progress(const struct pbl_display *disp) {
  return disp->state->busy;
}
