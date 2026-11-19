/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/util/uuid.h>

/**
 * @defgroup pp_transport_gatt GATT transport
 * @ingroup pp_transport
 * @brief Pebble protocol over GATT (PPoGATT), a client of the phone's PPoGATT service.
 *
 * The kernel LE client dispatches the GATT events of the service to these functions.
 * @{
 */

/** @brief Characteristics of the PPoGATT service. */
enum pbl_pp_gatt_characteristic {
  PBL_PP_GATT_CHARACTERISTIC_DATA,
  PBL_PP_GATT_CHARACTERISTIC_META,
  PBL_PP_GATT_CHARACTERISTIC_NUM
};

/** @brief Start, once the Bluetooth stack is up. */
void pbl_pp_gatt_create(void);

/** @brief Stop, closing every session. */
void pbl_pp_gatt_destroy(void);

/**
 * @brief Look for the PPoGATT services of the phone, and open a session with each.
 *
 * The Pebble app hosts one, and so may other apps. KernelMain only.
 *
 * @param conn_handle The connection of the phone.
 */
void pbl_pp_gatt_discover(uint16_t conn_handle);

/**
 * @brief The services of the phone changed: look for its PPoGATT services again.
 *
 * A service that is where it was keeps its session; the session of one that moved or is gone
 * is closed. KernelMain only.
 */
void pbl_pp_gatt_handle_services_changed(void);

/** @brief Close the sessions with the PPoGATT services of the phone. */
void pbl_pp_gatt_invalidate_all_references(void);

/**
 * @brief Handle a PPoGATT service found on the phone. KernelMain only, as the other handlers.
 *
 * @param instance Which of the phone's PPoGATT services, counting from 0.
 */
void pbl_pp_gatt_handle_service_discovered(uint8_t instance);

/**
 * @brief Handle the answer to subscribing to, or unsubscribing from, the data characteristic.
 *
 * @param instance Which of the phone's PPoGATT services.
 * @param is_subscribed true for a subscription.
 * @param error 0, the ATT error code (positive) or a negative errno.
 */
void pbl_pp_gatt_handle_subscribe(uint8_t instance, bool is_subscribed, int error);

/**
 * @brief Handle the meta characteristic read, or a notification of the data characteristic.
 *
 * @param instance Which of the phone's PPoGATT services.
 * @param characteristic The characteristic.
 * @param value Value read or notified.
 * @param value_length Length of @p value in bytes.
 * @param error 0, the ATT error code (positive) or a negative errno.
 */
void pbl_pp_gatt_handle_read_or_notification(uint8_t instance,
                                             enum pbl_pp_gatt_characteristic characteristic,
                                             const uint8_t *value, size_t value_length, int error);

/** @brief Clear the count of disconnections caused by repeated resets. */
void pbl_pp_gatt_reset_disconnect_counter(void);

/** @} */
