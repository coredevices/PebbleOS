/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "system/status_codes.h"

/**
 * @defgroup drivers_mcu MCU information
 * @ingroup drivers
 * @brief Microcontroller identification.
 * @{
 */

/**
 * @brief Read the microcontroller's unique ID.
 *
 * @param[out] buf Buffer receiving the ID.
 * @param[in,out] buf_sz Size of @p buf in bytes; set to the ID length on success.
 * @retval S_SUCCESS ID read.
 * @retval E_OUT_OF_MEMORY @p buf is too small.
 */
StatusCode mcu_get_serial(void *buf, size_t *buf_sz);

/** @} */
