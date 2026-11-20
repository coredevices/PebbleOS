/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

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
 * @retval 0 ID read.
 * @retval -ENOSPC @p buf is too small.
 * @retval -errno Other errors.
 */
int mcu_get_serial(void *buf, size_t *buf_sz);

/** @} */
