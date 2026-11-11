/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/coredump/coredump.h>
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

/** @brief Memory as the LCPU sees it, from @c dma-ranges. */
struct pbl_rproc_sf32lb52_range {
  /** Address on the LCPU. */
  uintptr_t da;
  /** Address on this CPU. */
  uintptr_t pa;
  /** Size in bytes. */
  size_t size;
};

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
  /** LPSYS SRAM region the LCPU works in (@c sram). */
  struct pbl_mem_region sram;
  /** Memory as the LCPU sees it: LPSYS SRAM through its code bus, then HPSYS SRAM. */
  struct pbl_rproc_sf32lb52_range ranges[2];
  /** LCPU clock, and the APB clocks of LPSYS, set for the LCPU to load. */
  struct pbl_clk hclk;
  /** LPSYS APB clock. */
  struct pbl_clk pclk;
  /** LPSYS second APB clock. */
  struct pbl_clk pclk2;
  /** Low-power clock, from the RTC: the crystal or the RC oscillator. */
  struct pbl_clk lpclk;
  /** Blocks the radio calibration programs, by base address. */
  struct {
    /** Bluetooth RF controller (@c sifli,bt-rfc). */
    uintptr_t rfc;
    /** Bluetooth PHY (@c sifli,bt-phy). */
    uintptr_t phy;
    /** PHY sample capture window (@c sifli,bt-phy, second region). */
    uintptr_t capture;
    /** Bluetooth controller (@c sifli,bt-mac). */
    uintptr_t mac;
    /** ADC (@c sifli,gpadc). */
    uintptr_t gpadc;
    /** DMA controller (@c sifli,dmac). */
    uintptr_t dmac;
    /** LPSYS clock and reset controller (@c sifli,lpsys-rcc). */
    uintptr_t lpsys_rcc;
    /** HPSYS configuration (@c sifli,hpsys-cfg). */
    uintptr_t hpsys_cfg;
    /** Power management unit (@c sifli,pmuc). */
    uintptr_t pmuc;
  } rf;
};

/**
 * @brief The address at which the LCPU sees memory of this CPU.
 *
 * @param lcpu LCPU.
 * @param pa Address on this CPU.
 * @return The address on the LCPU, or 0 if it cannot reach @p pa.
 */
uintptr_t pbl_rproc_sf32lb52_da(const struct pbl_rproc_sf32lb52 *lcpu, uintptr_t pa);

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_rproc_ops pbl_rproc_sf32lb52_ops;
int pbl_rproc_sf32lb52_init(const struct pbl_device *dev);
void pbl_rproc_sf32lb52_coredump_prepare(void);

#ifdef CONFIG_COREDUMP
#define PBL_RPROC_SF32LB52_COREDUMP_DEFINE(sym, _sram_base, _sram_size)               \
  PBL_COREDUMP_LATE_REGION_DEFINE(sym##_sram, (const void *)(_sram_base), _sram_size, \
                                  pbl_rproc_sf32lb52_coredump_prepare)
#else
#define PBL_RPROC_SF32LB52_COREDUMP_DEFINE(sym, _sram_base, _sram_size) static_assert(true)
#endif
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
 * @param _sram_base Base of the LPSYS SRAM region the LCPU works in.
 * @param _sram_size Its size, in bytes.
 * @param _r0_da LCPU address of LPSYS SRAM through its code bus.
 * @param _r0_pa Its address on this CPU.
 * @param _r0_size Its size, in bytes.
 * @param _r1_da LCPU address of HPSYS SRAM.
 * @param _r1_pa Its address on this CPU.
 * @param _r1_size Its size, in bytes.
 * @param _hclk LCPU clock, from PBL_CLK().
 * @param _pclk LPSYS APB clock, from PBL_CLK().
 * @param _pclk2 LPSYS second APB clock, from PBL_CLK().
 */
#define PBL_RPROC_SF32LB52_DEFINE(                                                                 \
    sym, _name, _patch_base, _tx_chan, _rx_chan, _mem_base, _mem_size, _sram_base, _sram_size,     \
    _r0_da, _r0_pa, _r0_size, _r1_da, _r1_pa, _r1_size, _hclk, _pclk, _pclk2, _lpclk, _rfc, _phy,  \
    _capture, _mac, _gpadc, _dmac, _lpsys_rcc, _hpsys_cfg, _pmuc)                                  \
  PBL_RPROC_SF32LB52_COREDUMP_DEFINE(sym, _sram_base, _sram_size);                                 \
  PBL_RPROC_STATE_DEFINE(sym);                                                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_rproc_sf32lb52, struct pbl_rproc, rproc) {                   \
    .rproc =                                                                                       \
        PBL_RPROC_INIT(sym, _name, pbl_rproc_sf32lb52_init, nullptr, &pbl_rproc_sf32lb52_ops),     \
    .patch_base = _patch_base, .tx_chan = _tx_chan, .rx_chan = _rx_chan,                           \
    .mem = PBL_MEM_REGION(_mem_base, _mem_size), .sram = PBL_MEM_REGION(_sram_base, _sram_size),   \
    .ranges = {{(_r0_da), (_r0_pa), (_r0_size)}, {(_r1_da), (_r1_pa), (_r1_size)}}, .hclk = _hclk, \
    .pclk = _pclk, .pclk2 = _pclk2, .lpclk = _lpclk,                                               \
    .rf = {                                                                                        \
      .rfc = _rfc,                                                                                 \
      .phy = _phy,                                                                                 \
      .capture = _capture,                                                                         \
      .mac = _mac,                                                                                 \
      .gpadc = _gpadc,                                                                             \
      .dmac = _dmac,                                                                               \
      .lpsys_rcc = _lpsys_rcc,                                                                     \
      .hpsys_cfg = _hpsys_cfg,                                                                     \
      .pmuc = _pmuc                                                                                \
    },                                                                                             \
  }

/** @} */
