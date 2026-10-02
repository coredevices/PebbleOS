/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include <nrfx_twim.h>
#pragma GCC diagnostic pop

/**
 * @defgroup drivers_i2c_nrf5 nRF5 I2C
 * @ingroup drivers_i2c
 * @brief @ref drivers_i2c_hal implementation for the nRF5 TWIM peripheral.
 * @{
 */

/** @brief nRF5 bus configuration. */
typedef struct I2CBusHal {
  /** TWIM instance. */
  nrfx_twim_t twim;
  /** Bus clock frequency. */
  nrf_twim_frequency_t frequency;
} I2CBusHal;

/** @} */
