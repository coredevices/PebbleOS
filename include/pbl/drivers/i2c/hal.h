/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <board/board.h>

/**
 * @defgroup drivers_i2c_hal I2C HAL
 * @ingroup drivers_i2c
 * @brief Interface between the common I2C code and the per-SoC bus controller drivers.
 *
 * Called by the common code with the bus lock held.
 * @{
 */

/**
 * @brief Configure the bus controller, leaving it disabled.
 *
 * @param bus Bus.
 */
void i2c_hal_init(I2CBus *bus);

/**
 * @brief Enable the bus controller.
 *
 * @param bus Bus.
 */
void i2c_hal_enable(I2CBus *bus);

/**
 * @brief Disable the bus controller.
 *
 * @param bus Bus.
 */
void i2c_hal_disable(I2CBus *bus);

/**
 * @brief Check whether the bus controller is busy.
 *
 * @param bus Bus.
 * @return True if busy.
 */
bool i2c_hal_is_busy(I2CBus *bus);

/**
 * @brief Abort the current transfer.
 *
 * @param bus Bus.
 */
void i2c_hal_abort_transfer(I2CBus *bus);

/**
 * @brief Prepare for the transfer in the bus state.
 *
 * Called once per transfer, before the first i2c_hal_start_transfer().
 *
 * @param bus Bus.
 */
void i2c_hal_init_transfer(I2CBus *bus);

/**
 * @brief Start the transfer in the bus state.
 *
 * May be called again to retry after a NACK. The outcome is reported with
 * i2c_handle_transfer_event().
 *
 * @param bus Bus.
 */
void i2c_hal_start_transfer(I2CBus *bus);

/** @} */
