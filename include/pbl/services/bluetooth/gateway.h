/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

/**
 * @defgroup services_bluetooth_gateway Gateway
 * @ingroup services_bluetooth
 * @brief The clients of the services of the gateway, the phone: PPoGATT, Service Changed, ANCS
 * and AMS. They start when the link becomes the gateway's and stop when it is lost.
 * @{
 */

/** @brief Start with the stack. */
void bt_gateway_init(void);

/**
 * @brief Handle the gateway being connected or lost, on KernelMain.
 *
 * @param event A @c PEBBLE_BLE_CONNECTION_EVENT.
 */
void bt_gateway_handle_event(const PebbleEvent *event);

/**
 * @brief Make the clients look for the services of the gateway again, as when it indicates that
 * they changed. Callable from any thread.
 */
void bt_gateway_rediscover(void);

/** @} */
