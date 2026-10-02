/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_dis Device Information Service
 * @ingroup services_bluetooth
 * @brief Contents of the BLE Device Information Service.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_bt_dis_info;
/** @endcond */

/**
 * @brief Fill in the device information.
 *
 * Model number (hardware version), manufacturer, serial number, firmware version and SDK
 * version.
 *
 * @param[out] info Device information.
 */
void dis_get_info(struct pbl_bt_dis_info *info);

/** @} */
