/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @brief Initialize RC10K with periodic calibration.
 */
void rc10k_init(void);

/**
 * @brief Get the current RC10K frequency in Hz.
 * @retval Frequency in Hz.
 */
uint32_t rc10k_get_freq_hz(void);