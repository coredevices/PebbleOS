/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/flash.h>

/**
 * @defgroup drivers_flash_qspi_flash_part_definitions QSPI flash parts
 * @ingroup drivers_flash
 * @brief Description of a QSPI NOR flash part: instructions, status bits and timings.
 *
 * Each part driver defines one @ref QSPIFlashPart. Unused instructions are left at 0.
 * @{
 */

/** @brief Quad Enable bit location, as in the JESD216 Basic Flash Parameter Table, DWORD 15. */
typedef enum JESD216Dw15QerType {
  /** No Quad Enable bit. */
  JESD216_DW15_QER_NONE = 0,
  /** Bit 1 of status register 2, written together with status register 1. */
  JESD216_DW15_QER_S2B1v1 = 1,
  /** Bit 6 of status register 1. */
  JESD216_DW15_QER_S1B6 = 2,
  /** Bit 7 of status register 2. */
  JESD216_DW15_QER_S2B7 = 3,
  /** Bit 1 of status register 2, written together with status register 1. */
  JESD216_DW15_QER_S2B1v4 = 4,
  /** Bit 1 of status register 2, written together with status register 1. */
  JESD216_DW15_QER_S2B1v5 = 5,
  /** Bit 1 of status register 2, writable on its own. */
  JESD216_DW15_QER_S2B1v6 = 6,
} JESD216Dw15QerType;

/** @brief QSPI flash part description. */
typedef const struct QSPIFlashPart {
  /** Instruction opcodes. */
  struct {
    /** Fast read (1-1-1). */
    uint8_t fast_read;
    /** Fast read, double data rate. */
    uint8_t fast_read_ddr;
    /** Dual output read (1-1-2). */
    uint8_t read2o;
    /** Dual I/O read (1-2-2). */
    uint8_t read2io;
    /** Quad output read (1-1-4). */
    uint8_t read4o;
    /** Quad I/O read (1-4-4). */
    uint8_t read4io;
    /** Page program (1-1-1). */
    uint8_t pp;
    /** Dual input page program (1-1-2). */
    uint8_t pp2o;
    /** Quad input page program (1-1-4). */
    uint8_t pp4o;
    /** Quad I/O page program (1-4-4). */
    uint8_t pp4io;
    /** 4 KiB sector (subsector) erase. */
    uint8_t erase_sector_4k;
    /** 64 KiB block (sector) erase. */
    uint8_t erase_block_64k;
    /** Write enable. */
    uint8_t write_enable;
    /** Write disable. */
    uint8_t write_disable;
    /** Read status register 1. */
    uint8_t rdsr1;
    /** Read status register 2. */
    uint8_t rdsr2;
    /** Write status register (1, or 1 and 2). */
    uint8_t wrsr;
    /** Write status register 2. */
    uint8_t wrsr2;
    /** Erase suspend. */
    uint8_t erase_suspend;
    /** Erase resume. */
    uint8_t erase_resume;
    /** Enter deep power-down. */
    uint8_t enter_low_power;
    /** Exit deep power-down. */
    uint8_t exit_low_power;
    /** Enter quad (QPI) mode. */
    uint8_t enter_quad_mode;
    /** Exit quad (QPI) mode. */
    uint8_t exit_quad_mode;
    /** Reset enable. */
    uint8_t reset_enable;
    /** Reset. */
    uint8_t reset;
    /** Read JEDEC ID. */
    uint8_t qspi_id;
    /** Lock a block. */
    uint8_t block_lock;
    /** Read a block's lock status. */
    uint8_t block_lock_status;
    /** Unlock all blocks. */
    uint8_t block_unlock_all;
    /** Enable write protection. */
    uint8_t write_protection_enable;
    /** Read the write protection status. */
    uint8_t read_protection_status;
    /** Enter 4-byte address mode. */
    uint8_t en4b;
    /** Erase a security register. */
    uint8_t erase_sec;
    /** Program a security register. */
    uint8_t program_sec;
    /** Read a security register. */
    uint8_t read_sec;
  } instructions;
  /** Status register 1 bits. */
  struct {
    /** Write in progress. */
    uint8_t busy;
    /** Write enable latch. */
    uint8_t write_enable;
  } status_bit_masks;
  /** Status register 2 bits. */
  struct {
    /** Security register lock bits. */
    uint8_t sec_lock;
    /** Erase suspended. */
    uint8_t erase_suspend;
  } flag_status_bit_masks;
  /** Dummy cycles. */
  struct {
    /** For @c fast_read. */
    uint8_t fast_read;
    /** For @c fast_read_ddr. */
    uint8_t fast_read_ddr;
  } dummy_cycles;
  /** Block locking. */
  struct {
    /** Data must be sent with the @c block_lock instruction. */
    bool has_lock_data;
    /** Data sent with the @c block_lock instruction, if @c has_lock_data. */
    uint8_t lock_data;
    /** Value returned by @c block_lock_status for a locked block. */
    uint8_t locked_check;
    /** Mask applied to @c read_protection_status to check that protection is enabled. */
    uint8_t protection_enabled_mask;
  } block_lock;
  /** Security registers. */
  FlashSecurityRegisters sec_registers;
  /** Time after a reset before the part accepts commands, in milliseconds. */
  uint32_t reset_latency_ms;
  /** Time from an erase suspend until reads are allowed, in microseconds. */
  uint32_t suspend_to_read_latency_us;
  /** Time to enter deep power-down, in microseconds. */
  uint32_t standby_to_low_power_latency_us;
  /** Time to leave deep power-down, in microseconds. */
  uint32_t low_power_to_standby_latency_us;
  /** Double data rate fast read is supported. */
  bool supports_fast_read_ddr;
  /** Block locking is supported. */
  bool supports_block_lock;
  /** Quad Enable bit location. */
  JESD216Dw15QerType qer_type;
  /** Expected JEDEC ID. */
  uint32_t qspi_id_value;
  /** Size in bytes. */
  uint32_t size;
  /** Part name. */
  const char *name;
} QSPIFlashPart;

/** @} */
