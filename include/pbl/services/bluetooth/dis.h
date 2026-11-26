/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_dis Device Information Service
 * @ingroup services_bluetooth
 * @brief Contents of the BLE Device Information Service.
 * @{
 */

/**
 * @brief Give the service the device information.
 *
 * Model number (hardware version), manufacturer, serial number, firmware version and SDK
 * version.
 */
void bt_dis_init(void);

/** @} */
