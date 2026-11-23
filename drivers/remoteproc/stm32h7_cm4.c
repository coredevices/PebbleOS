/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/mbox.h>
#include <pbl/drivers/remoteproc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/reboot.h>
#include <pbl/kernel/sem.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

// Long enough for the Cortex-M4 to take the request from its interrupt
#define SHUTDOWN_ACK_TIMEOUT_MS 200U

struct pbl_rproc_stm32h7_cm4_state {
  struct pbl_sem acked;
  bool booted;
  bool shutdown_sent;
};

struct pbl_rproc_stm32h7_cm4 {
  struct pbl_rproc rproc;
  struct pbl_mbox_client cl;
  struct pbl_mbox_chan shutdown;
  uintptr_t holdboot;
  uint32_t holdboot_mask;
  uintptr_t boot_address;
  uint32_t boot_address_mask;
  uintptr_t flash;
  uint32_t flash_mask;
  bool auto_boot;
  struct pbl_rproc_stm32h7_cm4_state *state;
};

static const struct pbl_rproc_stm32h7_cm4 *prv_cm4(const struct pbl_rproc *rproc) {
  return PBL_CONTAINER_OF(rproc, const struct pbl_rproc_stm32h7_cm4, rproc);
}

static void prv_tx_done(const struct pbl_mbox_client *cl, const void *msg, int ret) {
  const struct pbl_rproc_stm32h7_cm4 *cm4 =
      PBL_CONTAINER_OF(cl, const struct pbl_rproc_stm32h7_cm4, cl);

  pbl_sem_give(&cm4->state->acked);
}

// Without an image at its boot address, as a reset vector there tells, the Cortex-M4 would lock up
// and keep the system out of Stop mode
static int prv_start(const struct pbl_rproc *rproc) {
  const struct pbl_rproc_stm32h7_cm4 *cm4 = prv_cm4(rproc);
  uintptr_t image = PBL_FIELD_GET(cm4->boot_address_mask, pbl_readl(cm4->boot_address)) << 16;
  uint32_t reset = pbl_readl(image + sizeof(uint32_t));

  if (cm4->state->shutdown_sent) {
    return -ESHUTDOWN;
  }

  if ((reset & 1U) == 0U || reset < image || reset >= CONFIG_FLASH_BASE + CONFIG_FLASH_SIZE) {
    return -ENOENT;
  }

  // It runs from the flash at once: the domain of the flash must not stop under it
  pbl_modifyl(0U, cm4->flash_mask, cm4->flash);
  pbl_modifyl(0U, cm4->holdboot_mask, cm4->holdboot);
  cm4->state->booted = true;

  return 0;
}

// The Cortex-M4 powers off, with nothing to wake it until the system starts again
static void prv_shutdown(const struct pbl_rproc_stm32h7_cm4 *cm4, bool wait) {
  struct pbl_rproc_stm32h7_cm4_state *st = cm4->state;

  if (!st->booted || st->shutdown_sent || cm4->shutdown.ctrl == nullptr) {
    return;
  }

  st->shutdown_sent = true;
  pbl_sem_reset(&st->acked);
  if (pbl_mbox_send(&cm4->shutdown, nullptr) == 0 && wait) {
    (void)pbl_sem_take(&st->acked, PBL_MSEC(SHUTDOWN_ACK_TIMEOUT_MS));
  }
}

static void prv_stop(const struct pbl_rproc *rproc) {
  prv_shutdown(prv_cm4(rproc), true);
}

static const struct pbl_rproc_ops s_ops = {
  .start = prv_start,
  .stop = prv_stop,
};

// The system only reaches Standby once the Cortex-M4 stops too: tell it, and wait for it to take
// the request when the power-off allows it
static void prv_sys_off(enum pbl_sys_off_mode mode, void *user_data) {
  prv_shutdown(user_data, mode == PBL_SYS_OFF_POWER_OFF_PREPARE);
}

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_rproc_stm32h7_cm4 *cm4 =
      PBL_CONTAINER_OF(dev, const struct pbl_rproc_stm32h7_cm4, rproc.dev);
  int ret;

  pbl_sem_init(&cm4->state->acked, 0U, 1U);

  if (cm4->shutdown.ctrl != nullptr) {
    ret = pbl_mbox_request(&cm4->shutdown, &cm4->cl);
    if (ret != 0) {
      return ret;
    }
  }

  if (cm4->auto_boot) {
    ret = prv_start(&cm4->rproc);
    if (ret != 0 && ret != -ENOENT) {
      return ret;
    }
  }

  return 0;
}

#define PBL_RPROC_STM32H7_CM4_DEFINE(sym, _name, _holdboot, _holdboot_off, _holdboot_mask,         \
                                     _boot_address, _boot_address_off, _boot_address_mask, _flash, \
                                     _flash_off, _flash_mask, _shutdown, _auto_boot)               \
  PBL_RPROC_STATE_DEFINE(sym);                                                                     \
  static struct pbl_rproc_stm32h7_cm4_state sym##_cm4_state;                                       \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rproc_stm32h7_cm4, struct pbl_rproc, rproc){                 \
    .rproc = PBL_RPROC_INIT(sym, _name, prv_init, nullptr, &s_ops),                                \
    .cl = {.tx_done = prv_tx_done},                                                                \
    .shutdown = _shutdown,                                                                         \
    .holdboot = (_holdboot) + (_holdboot_off),                                                     \
    .holdboot_mask = (_holdboot_mask),                                                             \
    .boot_address = (_boot_address) + (_boot_address_off),                                         \
    .boot_address_mask = (_boot_address_mask),                                                     \
    .flash = (_flash) + (_flash_off),                                                              \
    .flash_mask = (_flash_mask),                                                                   \
    .auto_boot = (_auto_boot),                                                                     \
    .state = &sym##_cm4_state,                                                                     \
  };                                                                                               \
  PBL_SYS_OFF_HANDLER_DEFINE(sym##_prepare, PBL_SYS_OFF_POWER_OFF_PREPARE,                         \
                             PBL_SYS_OFF_PRIO_DEFAULT, prv_sys_off, (void *)&sym##_inst);          \
  PBL_SYS_OFF_HANDLER_DEFINE(sym##_power_off, PBL_SYS_OFF_POWER_OFF, PBL_SYS_OFF_PRIO_HIGH,        \
                             prv_sys_off, (void *)&sym##_inst)

#include PBL_DT_INSTANCES(st_stm32h747_m4)
