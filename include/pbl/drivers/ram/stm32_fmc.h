/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/ram.h>

/**
 * @defgroup drivers_ram_stm32_fmc STM32H7 FMC SDRAM
 * @ingroup drivers_ram
 * @brief @ref drivers_ram driver for an SDRAM on the STM32H7 FMC.
 *
 * The device brings the SDRAM up when it comes up and keeps it powered and refreshed. Its
 * address gives the FMC bank and the mapping: bank 1 at 0xc0000000 or, swapped, 0x60000000,
 * bank 2 at 0xd0000000 or 0x70000000. The swapped addresses are normal memory, cached and
 * executable by default. The timings come from the part, at the rate of the SDRAM clock: the
 * FMC kernel clock divided by 2 or 3, whichever first stays within 100 MHz.
 * @{
 */

/** @brief An SDRAM part. */
struct pbl_ram_sdram_part {
  /** Name. */
  const char *name;
  /** Internal banks, 2 or 4. */
  uint8_t banks;
  /** Row address bits, 11 to 13. */
  uint8_t row_bits;
  /** Column address bits, 8 to 11. */
  uint8_t col_bits;
  /** Data bus width, 8, 16 or 32 bits. */
  uint8_t width;
  /** CAS latency, in clock cycles, 1 to 3. */
  uint8_t cas_latency;
  /** Load mode register to active, in clock cycles. */
  uint8_t t_mrd_ck;
  /** Write recovery, in clock cycles. */
  uint8_t t_wr_ck;
  /** Exit self-refresh to active, in nanoseconds. */
  uint16_t t_xsr_ns;
  /** Active to precharge, in nanoseconds. */
  uint16_t t_ras_ns;
  /** Active to active of a bank, in nanoseconds. */
  uint16_t t_rc_ns;
  /** Precharge to active, in nanoseconds. */
  uint16_t t_rp_ns;
  /** Active to read or write, in nanoseconds. */
  uint16_t t_rcd_ns;
  /** Time in which every row is refreshed, in milliseconds. */
  uint16_t refresh_ms;
};

/** @brief ISSI IS42S32800J, 32 MiB, 32 bits. */
extern const struct pbl_ram_sdram_part pbl_ram_sdram_is42s32800j;

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
  /** Part. */
  const struct pbl_ram_sdram_part *part;
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
 * @param _part Part, a @ref pbl_ram_sdram_part.
 */
#define PBL_RAM_STM32_FMC_DEFINE(sym, _name, _base, _clk, _pinctrl, _addr, _size, _part)         \
  PBL_RAM_STATE_DEFINE(sym);                                                                     \
  PBL_DEVICE_INSTANCE(sym, struct pbl_ram_stm32_fmc, struct pbl_ram, ram) {                      \
    .ram = PBL_RAM_INIT(sym, _name, nullptr, &pbl_ram_stm32_fmc_ops), .base = (_base),           \
    .clk = _clk, .pinctrl = PBL_PINCTRL(_pinctrl, nullptr, 0), .addr = (_addr), .size = (_size), \
    .part = (_part),                                                                             \
  }

/** @} */
