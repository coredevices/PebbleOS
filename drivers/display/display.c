/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/ram.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/timer.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_display, CONFIG_DRIVER_DISPLAY_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_display_class, "display");

struct prv_sync {
  struct pbl_sem done;
  int status;
};

static uint32_t prv_pitch(const struct pbl_display_props *props) {
  switch (props->format) {
    case PBL_DISPLAY_FORMAT_MONO:
      return PBL_DIV_ROUND_UP(props->width, 8U);
    case PBL_DISPLAY_FORMAT_RGB565:
      return props->width * 2U;
    default:
      return props->width;
  }
}

// A framebuffer in a RAM device keeps it powered, for as long as the display is there
static int prv_framebuffer_init(const struct pbl_display *disp) {
  if (disp->framebuffer == nullptr) {
    return 0;
  }

  if (disp->framebuffer_size < prv_pitch(&disp->props) * disp->props.height) {
    return -EINVAL;
  }

#ifdef CONFIG_RAM
  if (disp->framebuffer_ram != nullptr) {
    int ret;

    ret = pbl_device_init(&disp->framebuffer_ram->dev);
    if (ret != 0) {
      return ret;
    }

    ret = pbl_ram_get(disp->framebuffer_ram);
    if (ret != 0) {
      return ret;
    }
  }
#endif

  memset(disp->framebuffer, 0, disp->framebuffer_size);

  return 0;
}

static void prv_framebuffer_deinit(const struct pbl_display *disp) {
#ifdef CONFIG_RAM
  if (disp->framebuffer != nullptr && disp->framebuffer_ram != nullptr) {
    pbl_ram_put(disp->framebuffer_ram);
  }
#endif
}

// An update its driver does not complete is stopped, and ends with the timeout
static void prv_timeout(struct pbl_timer *timer) {
  struct pbl_display_state *state = PBL_CONTAINER_OF(timer, struct pbl_display_state, timeout);
  const struct pbl_display *disp = state->disp;
  pbl_display_callback_t cb;
  void *arg;

  pbl_irq_lock();
  if (!state->busy) {
    pbl_irq_unlock();
    return;
  }
  disp->ops->abort(disp);
  state->busy = false;
  cb = state->cb;
  arg = state->arg;
  pbl_irq_unlock();

  PBL_LOG_WRN("display: update timed out");
  cb(-ETIMEDOUT, arg);
}

int pbl_display_dev_init(const struct pbl_device *dev) {
  const struct pbl_display *disp = PBL_CONTAINER_OF(dev, const struct pbl_display, dev);
  int ret;

  *disp->state = (struct pbl_display_state){.disp = disp};
  pbl_timer_init(&disp->state->timeout, prv_timeout);

  ret = prv_framebuffer_init(disp);
  if (ret != 0) {
    return ret;
  }

  ret = disp->ops->init(disp);
  if (ret == 0) {
    ret = disp->ops->set_rotated(disp, disp->props.rotated);
  }
  if (ret != 0) {
    prv_framebuffer_deinit(disp);
  }

  return ret;
}

// An update that timed out was completed already
void pbl_display_complete(const struct pbl_display *disp, int status) {
  struct pbl_display_state *state = disp->state;
  pbl_display_callback_t cb;
  void *arg;

  pbl_irq_lock();
  if (!state->busy) {
    pbl_irq_unlock();
    return;
  }
  if (disp->ops->abort != nullptr) {
    pbl_timer_stop(&state->timeout);
  }
  state->busy = false;
  cb = state->cb;
  arg = state->arg;
  pbl_irq_unlock();

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

  ret = disp->ops->set_rotated(disp, rotated != disp->props.rotated);
  if (ret != 0) {
    return ret;
  }

  disp->state->rotated = rotated;

  return 0;
}

int pbl_display_get_framebuffer(const struct pbl_display *disp, struct pbl_display_buffer *buf) {
  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  if (disp->framebuffer == nullptr) {
    return -ENOENT;
  }

  *buf = (struct pbl_display_buffer){
    .data = disp->framebuffer,
    .pitch = prv_pitch(&disp->props),
  };

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

  if (y0 > y1 || y1 >= disp->props.height || y1 - y0 >= pbl_display_max_update_rows(disp)) {
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

  if (disp->ops->abort != nullptr) {
    pbl_timer_start(&state->timeout, PBL_MSEC(CONFIG_DISPLAY_UPDATE_TIMEOUT_MS));
  }

  ret = disp->ops->update(disp, buf, y0, y1);
  if (ret != 0) {
    pbl_irq_lock();
    if (disp->ops->abort != nullptr) {
      pbl_timer_stop(&state->timeout);
    }
    state->busy = false;
    pbl_irq_unlock();
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
  uint16_t rows;
  int ret = 0;

  if (!pbl_device_is_ready(&disp->dev)) {
    return -ENODEV;
  }

  if (y0 > y1 || y1 >= disp->props.height) {
    return -EINVAL;
  }

  rows = pbl_display_max_update_rows(disp);

  pbl_sem_init(&sync.done, 0, 1);

  // One band after the other, each up to the rows the display takes
  for (uint32_t y = y0; y <= y1; y += rows) {
    uint16_t band_y1 = (uint16_t)PBL_MIN(y + rows - 1U, (uint32_t)y1);

    ret = pbl_display_update(disp, buf, (uint16_t)y, band_y1, prv_sync_complete, &sync);
    if (ret == 0) {
      (void)pbl_sem_take(&sync.done, PBL_FOREVER);
      ret = sync.status;
    }
    if (ret != 0) {
      break;
    }
  }

  pbl_sem_deinit(&sync.done);

  return ret;
}

bool pbl_display_update_in_progress(const struct pbl_display *disp) {
  return disp->state->busy;
}

// The panels keep their image while powered, which a power-off may leave them
static void prv_power_off_prepare(enum pbl_sys_off_mode mode, void *user_data) {
  const struct pbl_device *dev;

  PBL_DEVICE_CLASS_FOREACH(&pbl_display_class, dev) {
    const struct pbl_display *disp = PBL_CONTAINER_OF(dev, const struct pbl_display, dev);

    (void)pbl_display_clear(disp);
    (void)pbl_display_disable(disp);
  }
}

PBL_SYS_OFF_HANDLER_DEFINE(display_power_off, PBL_SYS_OFF_POWER_OFF_PREPARE,
                           PBL_SYS_OFF_PRIO_DEFAULT, prv_power_off_prepare, nullptr);
