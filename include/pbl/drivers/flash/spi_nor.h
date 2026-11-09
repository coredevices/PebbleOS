/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/drivers/flash/nor.h>
#include <pbl/drivers/spi_mem.h>
#include <pbl/util/size.h>

/**
 * @defgroup drivers_flash_spi_nor SPI NOR
 * @ingroup drivers_flash
 * @brief @ref drivers_flash driver for a JEDEC NOR part on a @ref drivers_spi_mem controller.
 *
 * The part is found by its JEDEC ID among the supported ones, and fails to initialize when it
 * is none of them. It is described by its SFDP tables (JESD216): size, page size, erase types
 * and times, read modes and 4-byte address opcodes. The part description fills in what SFDP does
 * not cover (OTP organization, status register layout and latencies) and stands in for SFDP on
 * parts without it. The widest read and program modes both the part and the controller support are
 * used. Parts over 16 MiB are driven with 4-byte address opcodes when both support them, which
 * leaves the part in its power-on 3-byte address mode; otherwise the part is put in 4-byte
 * address mode, and taken out of it by pbl_flash_stop().
 *
 * Reads go through the controller's memory-mapped window when it has one, and through operations
 * beyond it. On a @ref PBL_SPI_MEM_XIP controller the part is not reset, the state the boot code
 * left is adopted, and programs and erases return once the part is readable again. Addresses
 * start at the base the board gives, e.g. where the controller maps the part.
 *
 * The user OTP area is made of the part's security registers, laid out in cells:
 *
 * @code{.c}
 * PBL_FLASH_SPI_NOR_DEFINE(s_flash, "flash", &s_qspi, PBL_FLASH_OTP_CELL("serial-number", 0x20,
 * 32));
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_flash_spi_nor_data {
  const struct pbl_flash_nor_part *part;
  struct pbl_flash_geometry geometry;
  struct pbl_flash_otp_info otp;
  struct pbl_spi_mem_dirmap dirmap;
  struct pbl_spi_mem_pm pm;
  struct pbl_spi_mem_suspend susp;
  uint8_t addr_nbytes;
  uint8_t reg_addr_nbytes;
  uint8_t qer;
  uint8_t pp_opcode;
  uint8_t pp_data_width;
  uint8_t sector_opcode;
  uint8_t subsector_opcode;
  bool en4b;
  bool ex4b_wren;
  bool initialized;
};
/** @endcond */

/** @brief NOR flash on a SPI memory controller. */
struct pbl_flash_spi_nor {
  /** Flash. */
  struct pbl_flash flash;
  /** Driver state. */
  struct pbl_flash_spi_nor_data *data;
  /** Controller. */
  const struct pbl_spi_mem *bus;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_flash_ops pbl_flash_spi_nor_ops;
/** @endcond */

/**
 * @brief Define a NOR flash on a SPI memory controller.
 *
 * @param sym Symbol of the flash.
 * @param _name Name.
 * @param _bus Controller, a struct pbl_spi_mem.
 * @param ... OTP cells, from PBL_FLASH_OTP_CELL(), if any.
 */
#define PBL_FLASH_SPI_NOR_DEFINE(sym, _name, _bus, ...)                                 \
  PBL_FLASH_STATE_DEFINE(sym);                                                          \
  static struct pbl_flash_spi_nor_data sym##_data;                                      \
  __VA_OPT__(static const struct pbl_flash_otp_cell sym##_otp_cells[] = {__VA_ARGS__};) \
  static const struct pbl_flash_otp_layout sym##_otp_layout = {                         \
    __VA_OPT__(.cells = sym##_otp_cells, .num_cells = ARRAY_LENGTH(sym##_otp_cells))    \
  };                                                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_flash_spi_nor, struct pbl_flash, flash) {         \
    .flash = PBL_FLASH_INIT(sym, _name, &(_bus)->dev, nullptr, &pbl_flash_spi_nor_ops,  \
                            &sym##_data.geometry, &sym##_data.otp, &sym##_otp_layout),  \
    .data = &sym##_data, .bus = (_bus),                                                 \
  }

/** @} */
