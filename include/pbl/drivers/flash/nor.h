/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_flash_nor NOR flash parts
 * @ingroup drivers_flash
 * @brief Description of the SPI NOR parts boards are fitted with.
 *
 * Parts share the command set below; a part only describes what differs between them.
 * @{
 */

/**
 * @name Commands
 * @{
 */
/** Write enable. */
#define PBL_FLASH_NOR_CMD_WREN 0x06U
/** Read status register 1. */
#define PBL_FLASH_NOR_CMD_RDSR 0x05U
/** Read status register 2. */
#define PBL_FLASH_NOR_CMD_RDSR2 0x35U
/** Write status registers 1 and 2. */
#define PBL_FLASH_NOR_CMD_WRSR 0x01U
/** Write status register 2. */
#define PBL_FLASH_NOR_CMD_WRSR2 0x31U
/** Read JEDEC ID. */
#define PBL_FLASH_NOR_CMD_RDID 0x9FU
/** Read SFDP. */
#define PBL_FLASH_NOR_CMD_RDSFDP 0x5AU
/** Fast read (1-1-1). */
#define PBL_FLASH_NOR_CMD_FAST_READ 0x0BU
/** Page program (1-1-1). */
#define PBL_FLASH_NOR_CMD_PP 0x02U
/** Quad input page program (1-1-4). */
#define PBL_FLASH_NOR_CMD_PP_1_1_4 0x32U
/** 4 KiB erase. */
#define PBL_FLASH_NOR_CMD_SE 0x20U
/** 64 KiB erase. */
#define PBL_FLASH_NOR_CMD_BE 0xD8U
/** 4 KiB erase, 4-byte address. */
#define PBL_FLASH_NOR_CMD_SE_4B 0x21U
/** 64 KiB erase, 4-byte address. */
#define PBL_FLASH_NOR_CMD_BE_4B 0xDCU
/** Enter 4-byte address mode. */
#define PBL_FLASH_NOR_CMD_EN4B 0xB7U
/** Exit 4-byte address mode. */
#define PBL_FLASH_NOR_CMD_EX4B 0xE9U
/** Reset enable. */
#define PBL_FLASH_NOR_CMD_RSTEN 0x66U
/** Reset. */
#define PBL_FLASH_NOR_CMD_RST 0x99U
/** Erase suspend. */
#define PBL_FLASH_NOR_CMD_SUSPEND 0x75U
/** Erase resume. */
#define PBL_FLASH_NOR_CMD_RESUME 0x7AU
/** Enter deep power-down. */
#define PBL_FLASH_NOR_CMD_DPD 0xB9U
/** Release from deep power-down. */
#define PBL_FLASH_NOR_CMD_RDPD 0xABU
/** Erase a security register. */
#define PBL_FLASH_NOR_CMD_ERSCUR 0x44U
/** Program a security register. */
#define PBL_FLASH_NOR_CMD_PRSCUR 0x42U
/** Read a security register. */
#define PBL_FLASH_NOR_CMD_RDSCUR 0x48U
/** @} */

/**
 * @name Status register bits
 * @{
 */
/** Status register 1: write in progress. */
#define PBL_FLASH_NOR_SR_WIP PBL_BIT(0)
/** Status register 1: quad enable, on @ref PBL_FLASH_NOR_QE_SR1_BIT6 parts. */
#define PBL_FLASH_NOR_SR_QE PBL_BIT(6)
/** Status register 2: quad enable, on the other parts with one. */
#define PBL_FLASH_NOR_SR2_QE PBL_BIT(1)
/** Status register 2: erase suspended. */
#define PBL_FLASH_NOR_SR2_SUS PBL_BIT(7)
/** @} */

/** @brief Quad enable bit location, as in JESD216 BFPT DWORD 15. */
enum pbl_flash_nor_qe {
  /** No quad enable bit. */
  PBL_FLASH_NOR_QE_NONE,
  /** Status register 2 bit 1, written together with status register 1. */
  PBL_FLASH_NOR_QE_SR2_BIT1_WR_SR1,
  /** Status register 1 bit 6. */
  PBL_FLASH_NOR_QE_SR1_BIT6,
  /** Status register 2 bit 1, written on its own. */
  PBL_FLASH_NOR_QE_SR2_BIT1,
};

/**
 * @brief Organization of the security registers that make up the user OTP area.
 *
 * Region @c i is the security register at @c base + @c i * @c offset, read with 48h, programmed
 * with 42h and erased with 44h, and locked by status register 2 bit @c lb_bit + @c i.
 */
struct pbl_flash_nor_otp {
  /** Region size and count. */
  struct pbl_flash_otp_info info;
  /** Address of the first region. */
  uint32_t base;
  /** Distance between regions. */
  uint32_t offset;
  /** Status register 2 lock bit of the first region. */
  uint8_t lb_bit;
};

/**
 * @brief Initializer for a struct pbl_flash_nor_otp.
 *
 * @param _len Region size, in bytes.
 * @param _n Number of regions.
 * @param _base Address of the first region.
 * @param _offset Distance between regions.
 * @param _lb_bit Status register 2 lock bit of the first region.
 */
#define PBL_FLASH_NOR_OTP(_len, _n, _base, _offset, _lb_bit) \
  {                                                          \
    .info = {.region_len = (_len), .n_regions = (_n)},       \
    .base = (_base),                                         \
    .offset = (_offset),                                     \
    .lb_bit = (_lb_bit),                                     \
  }

/** @brief The part has 4-byte address opcodes (13h, 0Ch, 12h, 34h, 21h, DCh, ...). */
#define PBL_FLASH_NOR_F_4B_OPCODES PBL_BIT(0)

/** @brief A NOR part. */
struct pbl_flash_nor_part {
  /** Name. */
  const char *name;
  /** JEDEC ID as read, manufacturer in the least significant byte. */
  uint32_t id;
  /** Flags, @c PBL_FLASH_NOR_F_*. */
  uint32_t flags;
  /** Geometry. */
  struct pbl_flash_geometry geometry;
  /** User OTP area. */
  struct pbl_flash_nor_otp otp;
  /** Quad enable bit location, and how status register 2 is written. Overrides SFDP. */
  enum pbl_flash_nor_qe qe;
  /** Block protection bits of status register 1, cleared at init. */
  uint8_t sr1_bp_mask;
  /** Time from a reset until commands are accepted, in milliseconds. */
  uint16_t reset_ms;
  /** Time from an erase suspend until reads are allowed, in microseconds. */
  uint16_t suspend_us;
  /** Shortest time from an erase resume to the next suspend, in microseconds. */
  uint16_t resume_us;
  /** Time to enter deep power-down, in microseconds. */
  uint16_t dpd_enter_us;
  /** Time to leave deep power-down, in microseconds. */
  uint16_t dpd_exit_us;
};

/** @brief GigaDevice GD25LQ255E. */
extern const struct pbl_flash_nor_part pbl_flash_nor_gd25lq255e;

/** @brief GigaDevice GD25Q256E. */
extern const struct pbl_flash_nor_part pbl_flash_nor_gd25q256e;

/** @} */
