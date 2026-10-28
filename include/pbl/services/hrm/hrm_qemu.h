/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/services/hrm/hrm_manager.h>

/**
 * @defgroup services_hrm_qemu QEMU HRM backend
 * @ingroup services_hrm
 * @brief Heart rate backend for QEMU, which measures the synthetic pulse of the QEMU front end.
 * @{
 */

/**
 * @brief Simulate a wrist with the given heart rate and signal quality.
 *
 * The QEMU front end produces the pulse and the backend measures it back, so the reading follows
 * after a few beats.
 *
 * @param bpm Heart rate, in beats per minute.
 * @param quality Signal quality; HRMQuality_OffWrist removes the wrist.
 */
void hrm_qemu_set_heart_rate(uint8_t bpm, HRMQuality quality);

/** @} */
