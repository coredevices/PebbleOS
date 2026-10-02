/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_prf_update PRF update
 * @ingroup services
 * @brief Installation of a new recovery firmware (PRF).
 * @{
 */

/**
 * @brief Install a pending recovery firmware.
 *
 * If the new-PRF boot bit is set, clears it and copies the validated recovery image from the
 * firmware scratch region to the PRF slot. Does not copy when running the recovery firmware.
 */
void check_prf_update(void);

/** @} */
