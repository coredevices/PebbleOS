/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/ram.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/logging/logging.h>
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
      return (props->width + 7U) / 8U;
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

int pbl_display_dev_init(const struct pbl_device *dev) {
  const struct pbl_display *disp = PBL_CONTAINER_OF(dev, const struct pbl_display, dev);
  int ret;

  *disp->state = (struct pbl_display_state){0};

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

  if (y0 > y1 || y1 >= disp->props.height) {
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
