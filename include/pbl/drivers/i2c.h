/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <board/board.h>

/**
 * @defgroup drivers_i2c I2C
 * @ingroup drivers
 * @brief I2C controller driver interface.
 *
 * Devices are addressed through the board's @c I2CSlavePort, which names the bus and the
 * device address. A bus is powered while it has users: bracket transfers with i2c_use() and
 * i2c_release(). Transfers block the calling task, are serialized per bus and must not be
 * issued from ISRs.
 *
 * @code{.c}
 * uint8_t id;
 * uint8_t fifo[6];
 *
 * i2c_use(I2C_ACCEL);
 * bool ok = i2c_read_register(I2C_ACCEL, REG_WHO_AM_I, &id) &&
 *           i2c_write_register(I2C_ACCEL, REG_CTRL1, CTRL1_ODR_50HZ) &&
 *           i2c_read_register_block(I2C_ACCEL, REG_OUT_X_L, sizeof(fifo), fifo);
 * i2c_release(I2C_ACCEL);
 * @endcode
 * @{
 */

/**
 * @brief Start using the bus a device is connected to.
 *
 * Reference counted per bus; the bus is enabled on the first user. Must be called before any
 * transfer to the device.
 *
 * @param slave Device, which identifies the bus.
 */
void i2c_use(I2CSlavePort *slave);

/**
 * @brief Stop using the bus a device is connected to.
 *
 * The bus is disabled when its last user releases it.
 *
 * @param slave Device, which identifies the bus.
 */
void i2c_release(I2CSlavePort *slave);

/**
 * @brief Reset the bus a device is connected to.
 *
 * Disables and re-enables the bus controller. The caller must be using the bus (i2c_use()).
 *
 * @param slave Device, which identifies the bus.
 */
void i2c_reset(I2CSlavePort *slave);

/**
 * @brief Recover a stuck bus by clocking SCL until SDA is released.
 *
 * Not supported by the current bus drivers, which always fail.
 *
 * @param slave Device, which identifies the bus.
 * @return True if SDA recovered.
 */
bool i2c_bitbang_recovery(I2CSlavePort *slave);

/**
 * @brief Read a register.
 *
 * @param slave Device to read from.
 * @param register_address Register address.
 * @param[out] result Register value.
 * @return True on success.
 */
bool i2c_read_register(I2CSlavePort *slave, uint8_t register_address, uint8_t *result);

/**
 * @brief Read consecutive registers.
 *
 * Sends the start register address, then reads after a repeated start.
 *
 * @param slave Device to read from.
 * @param register_address_start First register address.
 * @param read_size Number of bytes to read.
 * @param[out] result_buffer Destination buffer of at least @p read_size bytes.
 * @return True on success.
 */
bool i2c_read_register_block(I2CSlavePort *slave, uint8_t register_address_start,
                             uint32_t read_size, uint8_t *result_buffer);

/**
 * @brief Read a block of registers into a buffer the controller may fill through DMA.
 *
 * Like i2c_read_register_block(), but the bus may use DMA and let the CPU sleep through the
 * transfer. The buffer must be aligned to @ref DCACHE_LINE_SIZE_MAX and own the whole cache lines
 * it covers, @c DCACHE_ROUND_UP(read_size) bytes: declare it with @c PBL_ALIGNED(
 * DCACHE_LINE_SIZE_MAX) and that size.
 *
 * @param slave Device to read from.
 * @param register_address_start First register address.
 * @param read_size Number of bytes to read.
 * @param[out] result_buffer Destination buffer, see above.
 * @return True on success.
 */
bool i2c_read_register_block_dma(I2CSlavePort *slave, uint8_t register_address_start,
                                 uint32_t read_size, uint8_t *result_buffer);

/**
 * @brief Read data without sending a register address.
 *
 * @param slave Device to read from.
 * @param read_size Number of bytes to read.
 * @param[out] result_buffer Destination buffer of at least @p read_size bytes.
 * @return True on success.
 */
bool i2c_read_block(I2CSlavePort *slave, uint32_t read_size, uint8_t *result_buffer);

/**
 * @brief Write a register.
 *
 * @param slave Device to write to.
 * @param register_address Register address.
 * @param value Value to write.
 * @return True on success.
 */
bool i2c_write_register(I2CSlavePort *slave, uint8_t register_address, uint8_t value);

/**
 * @brief Write consecutive registers.
 *
 * @param slave Device to write to.
 * @param register_address_start First register address.
 * @param write_size Number of bytes to write.
 * @param buffer Data to write.
 * @return True on success.
 */
bool i2c_write_register_block(I2CSlavePort *slave, uint8_t register_address_start,
                              uint32_t write_size, const uint8_t *buffer);

/**
 * @brief Write data without sending a register address.
 *
 * @param slave Device to write to.
 * @param write_size Number of bytes to write.
 * @param buffer Data to write.
 * @return True on success.
 */
bool i2c_write_block(I2CSlavePort *slave, uint32_t write_size, const uint8_t *buffer);

/**
 * @brief Write data, then read data, with no other transfer on the bus in between.
 *
 * Two transfers made while holding the bus lock, for devices that need a command before a read.
 * The read is skipped if the write fails.
 *
 * @param slave Device to talk to.
 * @param write_size Number of bytes to write.
 * @param write_buffer Data to write.
 * @param read_size Number of bytes to read.
 * @param[out] read_buffer Destination buffer of at least @p read_size bytes.
 * @return True if both transfers succeeded.
 */
bool i2c_write_read_block(I2CSlavePort *slave, uint32_t write_size, const uint8_t *write_buffer,
                          uint32_t read_size, uint8_t *read_buffer);

/** @} */
