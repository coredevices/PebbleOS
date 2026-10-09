/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/hrm/hrm_activity_scene.h>
#include <pbl/services/hrm/hrm_manager.h>

#include <board/board.h>

/**
 * @defgroup drivers_hrm Heart rate monitor
 * @ingroup drivers
 * @brief Heart rate monitor (HRM) driver interface.
 *
 * Used by the HRM manager service; the driver reports readings back to it with
 * hrm_manager_new_data_cb(). The per-implementation @c HRMDevice definitions live in the HRM
 * driver subgroups.
 *
 * @code{.c}
 * hrm_init(HRM);
 * if (hrm_enable(HRM, HRMFeature_BPM, true)) {
 *   ...
 *   hrm_disable(HRM);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Initialize the HRM.
 *
 * @param dev HRM device.
 */
void hrm_init(HRMDevice *dev);

/**
 * @brief Enable the HRM.
 *
 * Samples the PPG functions needed for the requested features.
 *
 * @param dev HRM device.
 * @param features Bitmask of HRMFeature values to collect.
 * @param low_latency True for live sessions that need prompt updates (workout, foreground app);
 *                    false for background logging, where the FIFO can be drained less often to
 *                    save wakeups since only the final reading matters.
 * @return True if enabled, false if initialization failed.
 */
bool hrm_enable(HRMDevice *dev, HRMFeature features, bool low_latency);

/**
 * @brief Disable the HRM.
 *
 * @param dev HRM device.
 */
void hrm_disable(HRMDevice *dev);

/**
 * @brief Check whether the HRM is enabled.
 *
 * @param dev HRM device.
 * @return True if enabled.
 */
bool hrm_is_enabled(HRMDevice *dev);

/**
 * @brief Tell the HRM which activity is in progress.
 *
 * Lets the algorithm use a motion-appropriate mode. Safe to call at any time and idempotent.
 *
 * @param dev HRM device.
 * @param scene Current activity.
 */
void hrm_set_activity_scene(HRMDevice *dev, HRMActivityScene scene);

/** @} */
