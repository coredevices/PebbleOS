/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/qspi_definitions.h>

/**
 * @defgroup drivers_qspi QSPI
 * @ingroup drivers
 * @brief Quad-SPI controller: indirect transfers, status polling and memory-mapped mode.
 *
 * Used by QSPI flash drivers (see @ref drivers_flash_qspi_flash). The peripheral clock must be
 * enabled with qspi_use() around transfers.
 *
 * @code{.c}
 * uint8_t sr1;
 *
 * qspi_use(port);
 * qspi_indirect_read_no_addr(port, 0x05, 0, &sr1, 1, false);
 * qspi_release(port);
 * @endcode
 * @{
 */

/** @brief Base address of the memory-mapped flash region. */
#define QSPI_MMAP_BASE_ADDRESS ((uintptr_t)0x90000000)

/** @brief qspi_poll_bit() timeout meaning wait forever. */
#define QSPI_NO_TIMEOUT (0)

/**
 * @brief Enable the peripheral clock.
 *
 * @param dev QSPI port.
 */
void qspi_use(QSPIPort *dev);

/**
 * @brief Disable the peripheral clock.
 *
 * @param dev QSPI port.
 */
void qspi_release(QSPIPort *dev);

/**
 * @brief Issue an instruction and read its response, without an address phase.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 * @param dummy_cycles Dummy cycles before the data phase.
 * @param[out] buffer Buffer receiving the data.
 * @param length Number of bytes to read.
 * @param is_ddr Use double data rate mode.
 */
void qspi_indirect_read_no_addr(QSPIPort *dev, uint8_t instruction, uint8_t dummy_cycles,
                                void *buffer, uint32_t length, bool is_ddr);

/**
 * @brief Issue an instruction with an address and read the response.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 * @param addr Address to read from.
 * @param dummy_cycles Dummy cycles before the data phase.
 * @param[out] buffer Buffer receiving the data.
 * @param length Number of bytes to read.
 * @param is_ddr Use double data rate mode.
 */
void qspi_indirect_read(QSPIPort *dev, uint8_t instruction, uint32_t addr, uint8_t dummy_cycles,
                        void *buffer, uint32_t length, bool is_ddr);

/**
 * @brief Issue an instruction with an address and read the response using DMA.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 * @param start_addr Address to read from.
 * @param dummy_cycles Dummy cycles before the data phase.
 * @param[out] buffer Buffer receiving the data.
 * @param length Number of bytes to read.
 * @param is_ddr Use double data rate mode.
 */
void qspi_indirect_read_dma(QSPIPort *dev, uint8_t instruction, uint32_t start_addr,
                            uint8_t dummy_cycles, void *buffer, uint32_t length, bool is_ddr);

/**
 * @brief Issue an instruction followed by data, without an address phase.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 * @param buffer Data to write, or NULL for none.
 * @param length Number of bytes to write, or 0 for none.
 */
void qspi_indirect_write_no_addr(QSPIPort *dev, uint8_t instruction, const void *buffer,
                                 uint32_t length);

/**
 * @brief Issue an instruction with an address, followed by data.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 * @param addr Address to write to.
 * @param buffer Data to write, or NULL for none.
 * @param length Number of bytes to write, or 0 for none.
 */
void qspi_indirect_write(QSPIPort *dev, uint8_t instruction, uint32_t addr, const void *buffer,
                         uint32_t length);

/**
 * @brief Issue an instruction on a single data line, without address or data.
 *
 * @param dev QSPI port.
 * @param instruction Instruction to issue.
 */
void qspi_indirect_write_no_addr_1line(QSPIPort *dev, uint8_t instruction);

/**
 * @brief Poll a status register until bits are set or cleared.
 *
 * @param dev QSPI port.
 * @param instruction Instruction reading the status register.
 * @param bit_mask Bits to poll.
 * @param should_be_set Wait for the bits to be set (true) or cleared (false).
 * @param timeout_us Timeout in microseconds, or @ref QSPI_NO_TIMEOUT.
 * @return true if the condition was met before the timeout.
 */
bool qspi_poll_bit(QSPIPort *dev, uint8_t instruction, uint8_t bit_mask, bool should_be_set,
                   uint32_t timeout_us);

/**
 * @brief Enter memory-mapped mode.
 *
 * The data is then readable at @ref QSPI_MMAP_BASE_ADDRESS + @p addr.
 *
 * @param dev QSPI port.
 * @param instruction Read instruction to use.
 * @param addr Start address of the data accessed through the mapping.
 * @param dummy_cycles Dummy cycles before the data phase.
 * @param length Length of the data accessed through the mapping.
 * @param is_ddr Use double data rate mode.
 */
void qspi_mmap_start(QSPIPort *dev, uint8_t instruction, uint32_t addr, uint8_t dummy_cycles,
                     uint32_t length, bool is_ddr);

/**
 * @brief Leave memory-mapped mode.
 *
 * @param dev QSPI port.
 */
void qspi_mmap_stop(QSPIPort *dev);

/** @} */
