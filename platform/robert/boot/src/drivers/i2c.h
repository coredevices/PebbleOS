/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>

//! Start using the I2C bus to which \a slave is connected
//! Must be called before any other reads or writes to the slave are performed
//! @param slave    I2C slave reference, which will identify the bus to use
void i2c_use(I2CSlavePort *slave);

//! Stop using the I2C bus to which \a slave is connected
//! Call when done communicating with the slave
//! @param slave    I2C slave reference, which will identify the bus to release
void i2c_release(I2CSlavePort *slave);

//! Reset the slave
//! Will cycle the power to and re-initialize the bus to which \a slave is connected, if this is
//! supported for the bus.
//! @param slave    I2C slave reference, which will identify the bus to be reset
void i2c_reset(I2CSlavePort *slave);

//! Manually bang out the clock on the bus to which \a slave is connected until the data line
//! recovers for a period or we timeout waiting for it to recover
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave    I2C slave reference, which will identify the bus to be recovered
//! @return true if the data line recovered, false otherwise
bool i2c_bitbang_recovery(I2CSlavePort *slave);

//! Read the value of a register
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                I2C slave to communicate with
//! @param register_address     Address of register to read
//! @param result               Pointer to destination buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_read_register(I2CSlavePort *slave, uint8_t register_address, uint8_t *result);

//! Read a sequence of registers starting from \a register_address_start
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                    I2C slave to communicate with
//! @param register_address_start   Address of first register to read
//! @param read_size                Number of bytes to read
//! @param result_buffer            Pointer to destination buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_read_register_block(I2CSlavePort *slave, uint8_t register_address_start,
                             uint32_t read_size, uint8_t* result_buffer);

//! Read a block of data without sending a register address before doing so.
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                    I2C slave to communicate with
//! @param read_size                Number of bytes to read
//! @param result_buffer            Pointer to destination buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_read_block(I2CSlavePort *slave, uint32_t read_size, uint8_t* result_buffer);

//! Write to a register
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                I2C slave to communicate with
//! @param register_address     Address of register to write to
//! @param value                Data value to write
//! @return true if transfer succeeded, false if error occurred
bool i2c_write_register(I2CSlavePort *slave, uint8_t register_address, uint8_t value);

//! Write to a sequence of registers starting from \a register_address_start
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                    I2C slave to communicate with
//! @param register_address_start   Address of first register to read
//! @param write_size               Number of bytes to write
//! @param buffer                   Pointer to source buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_write_register_block(I2CSlavePort *slave, uint8_t register_address_start,
                              uint32_t write_size, const uint8_t* buffer);

//! Write a block of data without sending a register address before doing so.
//! Must not be called before \ref i2c_use has been called for the slave
//! @param slave                    I2C slave to communicate with
//! @param write_size               Number of bytes to write
//! @param buffer                   Pointer to source buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_write_block(I2CSlavePort *slave, uint32_t write_size, const uint8_t* buffer);
