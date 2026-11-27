/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

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
 * @brief The link became the gateway's: start the clients. Callable from any thread, they start
 * on KernelMain.
 *
 * @param conn_handle The connection.
 */
void bt_gateway_connected(uint16_t conn_handle);

/** @brief The gateway is lost: stop the clients. Callable from any thread. */
void bt_gateway_disconnected(void);

/**
 * @brief Make the clients look for the services of the gateway again, as when it indicates that
 * they changed. Callable from any thread.
 */
void bt_gateway_rediscover(void);

/** @} */
