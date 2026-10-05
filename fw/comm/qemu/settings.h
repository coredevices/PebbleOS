/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup comm_qemu_settings Settings
 * @ingroup comm_qemu
 * @brief Emulator settings passed by QEMU in the RTC backup registers.
 * @{
 */

/** @brief QEMU settings. */
typedef enum {
  /** Run the first boot logic (bool). */
  QemuSetting_FirstBootLogicEnable = 1,
  /** Start with the phone connected (bool). */
  QemuSetting_DefaultConnected = 2,
  /** Start with the charger plugged in (bool). */
  QemuSetting_DefaultPluggedIn = 3,
} QemuSetting;

/**
 * @brief Read the given setting.
 *
 * @return Setting value; boolean settings are non-zero when set.
 */
uint32_t qemu_setting_get(QemuSetting);

/** @} */
