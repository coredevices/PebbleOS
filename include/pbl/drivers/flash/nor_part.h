/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>

//! JESD216 BFP DW15 quad-enable requirement.
enum pbl_flash_nor_qer {
  PBL_FLASH_NOR_QER_NONE = 0,
  PBL_FLASH_NOR_QER_S2B1v1 = 1,
  PBL_FLASH_NOR_QER_S1B6 = 2,
  PBL_FLASH_NOR_QER_S2B7 = 3,
  PBL_FLASH_NOR_QER_S2B1v4 = 4,
  PBL_FLASH_NOR_QER_S2B1v5 = 5,
  PBL_FLASH_NOR_QER_S2B1v6 = 6,
};

//! Command set of a JEDEC SPI NOR part, for controllers that issue the
//! commands themselves.
struct pbl_flash_nor_part {
  const char *name;
  uint32_t id;
  struct pbl_flash_geometry geometry;
  struct {
    uint8_t write_enable;
    uint8_t rdsr1;
    uint8_t rdsr2;
    uint8_t wrsr;
    uint8_t wrsr2;
    uint8_t erase_suspend;
    uint8_t erase_resume;
    uint8_t enter_low_power;
    uint8_t exit_low_power;
    uint8_t reset_enable;
    uint8_t reset;
    uint8_t read_id;
    uint8_t en4b;
    uint8_t erase_sec;
    uint8_t program_sec;
    uint8_t read_sec;
  } cmd;
  struct {
    uint8_t sr1_busy;
    uint8_t sr2_erase_suspend;
  } mask;
  uint32_t reset_latency_ms;
  uint32_t suspend_to_read_latency_us;
  uint32_t standby_to_low_power_latency_us;
  uint32_t low_power_to_standby_latency_us;
  enum pbl_flash_nor_qer qer;
  struct pbl_flash_sec_regs sec_regs;
};

extern const struct pbl_flash_nor_part pbl_flash_nor_gd25lq255e;
extern const struct pbl_flash_nor_part pbl_flash_nor_gd25q256e;

//! The part selected by Kconfig for the board's flash.
#if defined(CONFIG_FLASH_GD25LQ255E)
#define PBL_FLASH_NOR_PART pbl_flash_nor_gd25lq255e
#elif defined(CONFIG_FLASH_GD25Q256E)
#define PBL_FLASH_NOR_PART pbl_flash_nor_gd25q256e
#endif
