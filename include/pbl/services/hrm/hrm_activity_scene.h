/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup services_hrm_hrm_activity_scene HRM activity scene
 * @ingroup services_hrm
 * @brief What the user is doing, so the sensor algorithm can use a motion-tuned model.
 *
 * The driver maps scenes to its algorithm's modes. Kept independent of boards and drivers so
 * the HRM manager private header can use it.
 * @{
 */

/** @brief Activity scene for the heart rate algorithm. */
typedef enum {
  /** Rest or background sampling: the algorithm's general-purpose mode. */
  HRMActivityScene_Default = 0,
  /** Walking. */
  HRMActivityScene_Walk,
  /** Running, including high heart rates. */
  HRMActivityScene_Run,
  /** Open or mixed high-intensity exercise. */
  HRMActivityScene_HighIntensity,
} HRMActivityScene;

/** @} */
