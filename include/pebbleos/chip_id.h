/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup pebbleos_chip_id Processor IDs
 * @ingroup pebbleos
 * @brief IDs of the processors of a multi-processor device.
 *
 * They tell apart the source of hashed log messages (the 2-bit core field of a packed log hash)
 * and of core dumps (the core number of the core dump image header). IDs must be unique within a
 * platform and fit in 2 bits, which limits a device to four log or core dump producing
 * processors.
 * @{
 */

/** @brief Main MCU, running PebbleOS. */
#define CORE_ID_MAIN_MCU 0
/** @brief Bluetooth controller. */
#define CORE_ID_BLE 1

/** @} */
