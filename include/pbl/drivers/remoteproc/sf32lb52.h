/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/device.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/mbox.h>
#include <pbl/drivers/remoteproc.h>

/**
 * @defgroup drivers_remoteproc_sf32lb52 SF32LB52 LCPU
 * @ingroup drivers_remoteproc
 * @brief SiFli SF32LB52 low-power CPU.
 *
 * The LCPU runs the Bluetooth controller from ROM, which the driver configures, patches and
 * calibrates on start. The controller exchanges HCI with the HCPU over byte rings signalled with
 * a pair of doorbells; the LCPU to HCPU ring sits in LPSYS RAM, the HCPU to LCPU one in HCPU RAM
 * reserved for it, whose address the driver hands to the ROM. The HCI transport takes the
 * doorbells and the region from here.
 * @{
 */

/** @brief SF32LB52 LCPU. */
struct pbl_rproc_sf32lb52 {
  /** Remote processor. */
  struct pbl_rproc rproc;
  /** ROM patch controller base address. */
  uintptr_t patch_base;
  /** Doorbell to the LCPU (@c mboxes entry @c tx). */
  struct pbl_mbox_chan tx_chan;
  /** Doorbell from the LCPU (@c mboxes entry @c rx). */
  struct pbl_mbox_chan rx_chan;
  /** HCPU RAM reserved for the HCPU to LCPU ring (@c memory-region). */
  struct pbl_mem_region mem;
  /** LCPU clock, and the APB clocks of LPSYS, set for the LCPU to load. */
  struct pbl_clk hclk;
  /** LPSYS APB clock. */
  struct pbl_clk pclk;
  /** LPSYS second APB clock. */
  struct pbl_clk pclk2;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rproc_ops pbl_rproc_sf32lb52_ops;
int pbl_rproc_sf32lb52_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the LCPU.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _patch_base ROM patch controller base address.
 * @param _tx_chan Doorbell to the LCPU, from PBL_MBOX_CHAN().
 * @param _rx_chan Doorbell from the LCPU, from PBL_MBOX_CHAN().
 * @param _mem_base Base of the HCPU RAM reserved for the HCPU to LCPU ring.
 * @param _mem_size Its size, in bytes.
 * @param _hclk LCPU clock, from PBL_CLK().
 * @param _pclk LPSYS APB clock, from PBL_CLK().
 * @param _pclk2 LPSYS second APB clock, from PBL_CLK().
 */
#define PBL_RPROC_SF32LB52_DEFINE(sym, _name, _patch_base, _tx_chan, _rx_chan, _mem_base,       \
                                  _mem_size, _hclk, _pclk, _pclk2)                              \
  PBL_RPROC_STATE_DEFINE(sym);                                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rproc_sf32lb52, struct pbl_rproc, rproc) {                \
    .rproc =                                                                                    \
        PBL_RPROC_INIT(sym, _name, pbl_rproc_sf32lb52_init, nullptr, &pbl_rproc_sf32lb52_ops),  \
    .patch_base = _patch_base, .tx_chan = _tx_chan, .rx_chan = _rx_chan,                        \
    .mem = PBL_MEM_REGION(_mem_base, _mem_size), .hclk = _hclk, .pclk = _pclk, .pclk2 = _pclk2, \
  }

/** @} */
