/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/remoteproc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

struct pbl_rproc_stm32h7_cm4 {
  struct pbl_rproc rproc;
  uintptr_t holdboot;
  uint32_t holdboot_mask;
  uintptr_t boot_address;
  uint32_t boot_address_mask;
  bool auto_boot;
};

static const struct pbl_rproc_stm32h7_cm4 *prv_cm4(const struct pbl_rproc *rproc) {
  return PBL_CONTAINER_OF(rproc, const struct pbl_rproc_stm32h7_cm4, rproc);
}

// Without an image at its boot address, as a reset vector there tells, the Cortex-M4 would lock up
static int prv_start(const struct pbl_rproc *rproc) {
  const struct pbl_rproc_stm32h7_cm4 *cm4 = prv_cm4(rproc);
  uintptr_t image = PBL_FIELD_GET(cm4->boot_address_mask, pbl_readl(cm4->boot_address)) << 16;
  uint32_t reset = pbl_readl(image + sizeof(uint32_t));

  if ((reset & 1U) == 0U || reset < image || reset >= CONFIG_FLASH_BASE + CONFIG_FLASH_SIZE) {
    return -ENOENT;
  }

  pbl_modifyl(0U, cm4->holdboot_mask, cm4->holdboot);

  return 0;
}

// Once let boot, the Cortex-M4 runs until it stops itself
static void prv_stop(const struct pbl_rproc *rproc) {
}

static const struct pbl_rproc_ops s_ops = {
  .start = prv_start,
  .stop = prv_stop,
};

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_rproc_stm32h7_cm4 *cm4 =
      PBL_CONTAINER_OF(dev, const struct pbl_rproc_stm32h7_cm4, rproc.dev);
  int ret;

  if (!cm4->auto_boot) {
    return 0;
  }

  ret = prv_start(&cm4->rproc);

  return ret == -ENOENT ? 0 : ret;
}

#define PBL_RPROC_STM32H7_CM4_DEFINE(sym, _name, _holdboot, _holdboot_off, _holdboot_mask, \
                                     _boot_address, _boot_address_off, _boot_address_mask, \
                                     _auto_boot)                                           \
  PBL_RPROC_STATE_DEFINE(sym);                                                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rproc_stm32h7_cm4, struct pbl_rproc, rproc) {        \
    .rproc = PBL_RPROC_INIT(sym, _name, prv_init, nullptr, &s_ops),                        \
    .holdboot = (_holdboot) + (_holdboot_off), .holdboot_mask = (_holdboot_mask),          \
    .boot_address = (_boot_address) + (_boot_address_off),                                 \
    .boot_address_mask = (_boot_address_mask), .auto_boot = (_auto_boot),                  \
  }

#include PBL_DT_INSTANCES(st_stm32h747_m4)
