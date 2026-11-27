/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/remoteproc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_spu.h>
#include <hardware/nrf54l_vpr.h>

// What erased RRAM reads
#define ERASED UINT32_MAX

struct pbl_rproc_nrf_vpr {
  struct pbl_rproc rproc;
  uintptr_t base;
  uintptr_t spu;
  uintptr_t firmware;
  size_t firmware_size;
  uintptr_t exec;
  size_t exec_size;
  bool auto_boot;
};

static const struct pbl_rproc_nrf_vpr *prv_vpr(const struct pbl_rproc *rproc) {
  return PBL_CONTAINER_OF(rproc, const struct pbl_rproc_nrf_vpr, rproc);
}

// The VPR runs its firmware from its first instruction on, in place or copied to its memory
// region, as a secure bus master so that it reaches the peripherals at the addresses this core
// does
static int prv_start(const struct pbl_rproc *rproc) {
  const struct pbl_rproc_nrf_vpr *vpr = prv_vpr(rproc);
  uintptr_t entry = vpr->firmware;

  if (pbl_readl(vpr->firmware) == ERASED) {
    return -ENOENT;
  }

  if (vpr->exec_size != 0U) {
    memcpy((void *)vpr->exec, (const void *)vpr->firmware,
           vpr->exec_size < vpr->firmware_size ? vpr->exec_size : vpr->firmware_size);
    entry = vpr->exec;
  }

  pbl_modifyl(0U, SPU_PERIPH_PERM_SECATTR,
              vpr->spu + NRF54L_SPU_PERIPH_PERM_OFFSET(SPU_SLAVE_INDEX(vpr->base)));
  pbl_writel(entry, vpr->base + NRF54L_VPR_INITPC_OFFSET);
  pbl_writel(VPR_CPURUN_EN, vpr->base + NRF54L_VPR_CPURUN_OFFSET);

  return 0;
}

// Stopped, the VPR starts again from its first instruction
static void prv_stop(const struct pbl_rproc *rproc) {
  const struct pbl_rproc_nrf_vpr *vpr = prv_vpr(rproc);

  pbl_writel(0U, vpr->base + NRF54L_VPR_CPURUN_OFFSET);
}

static const struct pbl_rproc_ops s_ops = {
  .start = prv_start,
  .stop = prv_stop,
};

static int prv_init(const struct pbl_device *dev) {
  const struct pbl_rproc_nrf_vpr *vpr =
      PBL_CONTAINER_OF(dev, const struct pbl_rproc_nrf_vpr, rproc.dev);
  int ret;

  if (!vpr->auto_boot) {
    return 0;
  }

  ret = prv_start(&vpr->rproc);

  return ret == -ENOENT ? 0 : ret;
}

#define PBL_RPROC_NRF_VPR_DEFINE(sym, _name, _base, _spu, _firmware, _firmware_size, _exec,        \
                                 _exec_size, _auto_boot)                                           \
  PBL_RPROC_STATE_DEFINE(sym);                                                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rproc_nrf_vpr, struct pbl_rproc, rproc) {                    \
    .rproc = PBL_RPROC_INIT(sym, _name, prv_init, nullptr, &s_ops), .base = (_base),               \
    .spu = (_spu), .firmware = CONFIG_FLASH_BASE + (_firmware), .firmware_size = (_firmware_size), \
    .exec = (_exec), .exec_size = (_exec_size), .auto_boot = (_auto_boot),                         \
  }

#include PBL_DT_INSTANCES(nordic_nrf54l15_vpr)
