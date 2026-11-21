/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/ram.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_ram_stm32_fmc STM32H7 FMC SDRAM
 * @ingroup drivers_ram
 * @brief @ref drivers_ram driver for an SDRAM on the STM32H7 FMC.
 *
 * The device brings the SDRAM up when it comes up and keeps it powered and refreshed. Its
 * address gives the FMC bank and the mapping: bank 1 at 0xc0000000 or, swapped, 0x60000000,
 * bank 2 at 0xd0000000 or 0x70000000. The swapped addresses are normal memory, cached and
 * executable by default. The SDRAM clock is the FMC kernel clock divided by 2 or 3, whichever
 * first stays within 100 MHz. The timings and the lowest CAS latency allowed at that clock come
 * from the SDRAM.
 * @{
 */

/** @brief A CAS latency of an SDRAM and the highest clock it allows. */
struct pbl_ram_sdram_cas_latency {
  /** CAS latency, in clock cycles, 1 to 3. */
  uint8_t cycles;
  /** Highest clock, in Hz. */
  uint32_t max_hz;
};

/** @brief A JEDEC SDR SDRAM. */
struct pbl_ram_sdram {
  /** Data bus width, 8, 16 or 32 bits. */
  uint8_t width;
  /** Internal banks, 2 or 4. */
  uint8_t banks;
  /** Row address bits, 11 to 13. */
  uint8_t row_bits;
  /** Column address bits, 8 to 11. */
  uint8_t col_bits;
  /** Active to active of a bank, in picoseconds. */
  uint32_t t_rc_ps;
  /** Active to precharge, in picoseconds. */
  uint32_t t_ras_ps;
  /** Precharge to active, in picoseconds. */
  uint32_t t_rp_ps;
  /** Active to read or write, in picoseconds. */
  uint32_t t_rcd_ps;
  /** Write recovery, in picoseconds. */
  uint32_t t_wr_ps;
  /** Load mode register to the next command, in picoseconds. */
  uint32_t t_mrd_ps;
  /** Exit self-refresh to active, in picoseconds. */
  uint32_t t_xsr_ps;
  /** Time in which every row is refreshed, in milliseconds. */
  uint16_t t_ref_ms;
  /** CAS latencies of the speed grade. */
  const struct pbl_ram_sdram_cas_latency *cas_latencies;
  /** Number of CAS latencies. */
  uint8_t num_cas_latencies;
};

/** @brief SDRAM on the FMC. */
struct pbl_ram_stm32_fmc {
  /** RAM device. */
  struct pbl_ram ram;
  /** FMC registers. */
  uintptr_t base;
  /** FMC kernel clock. */
  struct pbl_clk clk;
  /** Pins. */
  struct pbl_pinctrl pinctrl;
  /** CPU address of the SDRAM. */
  uintptr_t addr;
  /** Size of the SDRAM, in bytes. */
  size_t size;
  /** SDRAM. */
  struct pbl_ram_sdram sdram;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_ram_ops pbl_ram_stm32_fmc_ops;
/** @endcond */

/**
 * @brief Define an SDRAM on the FMC.
 *
 * @param sym Symbol of the device.
 * @param _name Name.
 * @param _base FMC registers.
 * @param _clk FMC kernel clock, from PBL_CLK().
 * @param _pinctrl Default pin control state.
 * @param _addr CPU address of the SDRAM.
 * @param _size Size of the SDRAM, in bytes.
 * @param _width Data bus width, in bits.
 * @param _banks Internal banks.
 * @param _row_bits Row address bits.
 * @param _col_bits Column address bits.
 * @param _t_rc_ps Active to active of a bank, in picoseconds.
 * @param _t_ras_ps Active to precharge, in picoseconds.
 * @param _t_rp_ps Precharge to active, in picoseconds.
 * @param _t_rcd_ps Active to read or write, in picoseconds.
 * @param _t_wr_ps Write recovery, in picoseconds.
 * @param _t_mrd_ps Load mode register to the next command, in picoseconds.
 * @param _t_xsr_ps Exit self-refresh to active, in picoseconds.
 * @param _t_ref_ms Refresh period, in milliseconds.
 * @param ... CAS latencies, each a @ref pbl_ram_sdram_cas_latency initializer.
 */
#define PBL_RAM_STM32_FMC_DEFINE(sym, _name, _base, _clk, _pinctrl, _addr, _size, _width, _banks, \
                                 _row_bits, _col_bits, _t_rc_ps, _t_ras_ps, _t_rp_ps, _t_rcd_ps,  \
                                 _t_wr_ps, _t_mrd_ps, _t_xsr_ps, _t_ref_ms, ...)                  \
  static const struct pbl_ram_sdram_cas_latency sym##_cas_latencies[] = {__VA_ARGS__};            \
  PBL_RAM_STATE_DEFINE(sym);                                                                      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ram_stm32_fmc, struct pbl_ram, ram) {                       \
    .ram = PBL_RAM_INIT(sym, _name, nullptr, &pbl_ram_stm32_fmc_ops), .base = (_base),            \
    .clk = _clk, .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0), .addr = (_addr), .size = (_size),  \
    .sdram = {                                                                                    \
      .width = (_width),                                                                          \
      .banks = (_banks),                                                                          \
      .row_bits = (_row_bits),                                                                    \
      .col_bits = (_col_bits),                                                                    \
      .t_rc_ps = (_t_rc_ps),                                                                      \
      .t_ras_ps = (_t_ras_ps),                                                                    \
      .t_rp_ps = (_t_rp_ps),                                                                      \
      .t_rcd_ps = (_t_rcd_ps),                                                                    \
      .t_wr_ps = (_t_wr_ps),                                                                      \
      .t_mrd_ps = (_t_mrd_ps),                                                                    \
      .t_xsr_ps = (_t_xsr_ps),                                                                    \
      .t_ref_ms = (_t_ref_ms),                                                                    \
      .cas_latencies = sym##_cas_latencies,                                                       \
      .num_cas_latencies = PBL_ARRAY_SIZE(sym##_cas_latencies),                                   \
    },                                                                                            \
  }

/** @} */
