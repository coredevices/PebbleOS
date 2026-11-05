/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/device.h>
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
 * doorbells and the region from here:
 *
 * @code{.c}
 * PBL_RPROC_SF32LB52_DEFINE(s_lcpu, "lcpu", PBL_MBOX_CHAN(&s_mailbox1.ctrl, 0),
 *                           PBL_MBOX_CHAN(&s_mailbox2.ctrl, 0),
 *                           PBL_MEM_REGION(SF32LB52_HPSYS_MBOX_BUF_BASE, SF32LB52_MBOX_BUF_SIZE));
 * @endcode
 * @{
 */

/** @brief SF32LB52 LCPU. */
struct pbl_rproc_sf32lb52 {
  /** Remote processor. */
  struct pbl_rproc rproc;
  /** Doorbell to the LCPU (@c mboxes entry @c tx). */
  struct pbl_mbox_chan tx_chan;
  /** Doorbell from the LCPU (@c mboxes entry @c rx). */
  struct pbl_mbox_chan rx_chan;
  /** HCPU RAM reserved for the HCPU to LCPU ring (@c memory-region). */
  struct pbl_mem_region mem;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rproc_ops pbl_rproc_sf32lb52_ops;
int pbl_rproc_sf32lb52_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define and register the LCPU.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _tx_chan Doorbell to the LCPU, a PBL_MBOX_CHAN().
 * @param _rx_chan Doorbell from the LCPU, a PBL_MBOX_CHAN().
 * @param _mem HCPU RAM reserved for the HCPU to LCPU ring, a PBL_MEM_REGION().
 */
#define PBL_RPROC_SF32LB52_DEFINE(sym, _name, _tx_chan, _rx_chan, _mem)                        \
  PBL_RPROC_STATE_DEFINE(sym);                                                                 \
  const struct pbl_rproc_sf32lb52 sym = {                                                      \
    .rproc =                                                                                   \
        PBL_RPROC_INIT(sym, _name, pbl_rproc_sf32lb52_init, nullptr, &pbl_rproc_sf32lb52_ops), \
    .tx_chan = _tx_chan,                                                                       \
    .rx_chan = _rx_chan,                                                                       \
    .mem = _mem,                                                                               \
  };                                                                                           \
  PBL_DEVICE_REGISTER(sym, &sym.rproc.dev)

/** @} */
