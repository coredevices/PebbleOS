/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>
#include <stdint.h>

//! Initialize the I2C driver. Must be called before first use
void i2c_init(void);

//! Start using the I2C bus connected to the device specified by \a device_id
//! Must be called before any other use of the bus is performed
//! @param device_id    ID of device
void i2c_use(I2cDevice device_id);

//! Stop using the I2C bus connected to the device specified by \a device_id
//! Call when done with the bus
//! @param device_id    ID of device
void i2c_release(I2cDevice device_id);

//! Reset the bus
//! Will re-initialize the bus and cycle the power to the bus if this is
//! supported for the bus the device specified by \a device_id is connected to)
//! @param device_id    ID of device, this will identify the bus to be reset
void i2c_reset(I2cDevice device_id);

//! Manually bang out the clock on the bus specified by \a device_id for a period
//! of time or until the data line recovers
//! Must not be called before \ref i2c_use has been called for the device
//! @param device_id    ID of device, this will identify the bus to be recovered
//! @return true if the data line recovered, false otherwise
bool i2c_bitbang_recovery(I2cDevice device_id);

//! Read the value of a register
//! Must not be called before \ref i2c_use has been called for the device
//! @param device_id            ID of device to communicate with
//! @param i2c_device_address   Device bus address
//! @param register_address     Address of register to read
//! @param result               Pointer to destination buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_read_register(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address, uint8_t *result);

//! Read a sequence of registers starting from \a register_address_start
//! Must not be called before \ref i2c_use has been called for the device
//! @param device_id                ID of device to communicate with
//! @param i2c_device_address       Device bus address
//! @param register_address_start   Address of first register to read
//! @param read_size                Number of bytes to read
//! @param result_buffer            Pointer to destination buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_read_register_block(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address_start, uint8_t read_size, uint8_t* result_buffer);

//! Write to a register
//! Must not be called before \ref i2c_use has been called for the device
//! @param device_id            ID of device to communicate with
//! @param i2c_device_address   Device bus address
//! @param register_address     Address of register to write to
//! @param value                Data value to write
//! @return true if transfer succeeded, false if error occurred
bool i2c_write_register(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address, uint8_t value);

//! Write to a sequence of registers starting from \a register_address_start
//! Must not be called before \ref i2c_use has been called for the device
//! @param device_id                ID of device to communicate with
//! @param i2c_device_address       Device bus address
//! @param register_address_start   Address of first register to read
//! @param write_size               Number of bytes to write
//! @param buffer                   Pointer to source buffer
//! @return true if transfer succeeded, false if error occurred
bool i2c_write_register_block(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address_start, uint8_t write_size, const uint8_t* buffer);
