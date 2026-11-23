/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/responsiveness.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup pp_transport_gatt GATT transport
 * @ingroup pp_transport
 * @brief Pebble protocol over GATT (PPoGATT), a client of the phone's PPoGATT service.
 *
 * The application starts it when the phone connects, and stops it when the phone goes. Unless
 * told otherwise, its functions are for the main thread, see pbl_pp_main_call().
 * @{
 */

/** @brief Length of the serial number sent to the phone, in characters. */
#define PBL_PP_GATT_SERIAL_NUMBER_SIZE 12

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
 * The Pebble app hosts one, and so may other apps.
 *
 * @param conn_handle The connection of the phone.
 */
void pbl_pp_gatt_discover(uint16_t conn_handle);

/**
 * @brief The services of the phone changed: look for its PPoGATT services again.
 *
 * A service that is where it was keeps its session; the session of one that moved or is gone
 * is closed.
 */
void pbl_pp_gatt_handle_services_changed(void);

/** @brief Close the sessions with the PPoGATT services of the phone. */
void pbl_pp_gatt_invalidate_all_references(void);

/**
 * @brief Handle a PPoGATT service found on the phone.
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

/**
 * @brief Get the serial number of the device, sent to the phone when a session starts.
 * Implemented by the application.
 *
 * @return @ref PBL_PP_GATT_SERIAL_NUMBER_SIZE characters; a terminator is not needed.
 */
const char *pbl_pp_gatt_cb_get_serial_number(void);

/**
 * @brief Apply a connection response time, see pbl_pp_session_set_responsiveness(). Implemented
 * by the application, and called from any thread.
 *
 * @param conn_handle The connection of the phone.
 * @param consumer Consumer making the request.
 * @param state Requested response time state.
 * @param max_period_secs Maximum time to stay in @p state.
 */
void pbl_pp_gatt_cb_set_responsiveness(uint16_t conn_handle, enum pbl_bt_consumer consumer,
                                       enum pbl_bt_response_time_state state,
                                       uint16_t max_period_secs);

/** @} */
