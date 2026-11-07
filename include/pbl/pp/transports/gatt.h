/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/util/uuid.h>

#include <comm/ble/gatt_client_subscriptions.h>

/**
 * @defgroup pp_transport_gatt GATT transport
 * @ingroup pp_transport
 * @brief Pebble protocol over GATT (PPoGATT), a client of the phone's PPoGATT service.
 *
 * The kernel LE client dispatches the GATT events of the service to these functions.
 * @{
 */

/** @brief Characteristics of the PPoGATT service, in discovery order. */
enum pbl_pp_gatt_characteristic {
  PBL_PP_GATT_CHARACTERISTIC_DATA,
  PBL_PP_GATT_CHARACTERISTIC_META,
  PBL_PP_GATT_CHARACTERISTIC_NUM
};

/** @brief UUID of the phone's PPoGATT service. */
extern const Uuid pbl_pp_gatt_service_uuid;

/** @brief UUIDs of the service's characteristics, indexed by @ref pbl_pp_gatt_characteristic. */
extern const Uuid pbl_pp_gatt_characteristic_uuids[PBL_PP_GATT_CHARACTERISTIC_NUM];

/** @brief Start, once the Bluetooth stack is up. */
void pbl_pp_gatt_create(void);

/** @brief Stop, closing every session. */
void pbl_pp_gatt_destroy(void);

/**
 * @brief Handle a discovered PPoGATT service.
 *
 * @param characteristics Handles of the characteristics, indexed by
 *                        @ref pbl_pp_gatt_characteristic.
 */
void pbl_pp_gatt_handle_service_discovered(pbl_bt_characteristic_t *characteristics);

/**
 * @brief Handle a removed PPoGATT service.
 *
 * @param characteristics Handles of the removed characteristics.
 * @param num_characteristics Number of handles.
 */
void pbl_pp_gatt_handle_service_removed(pbl_bt_characteristic_t *characteristics,
                                        uint8_t num_characteristics);

/** @brief Forget every characteristic handle, e.g. on disconnection. */
void pbl_pp_gatt_invalidate_all_references(void);

/**
 * @brief Check whether a characteristic belongs to a PPoGATT service.
 *
 * @param characteristic Characteristic handle.
 * @return true if the transport handles @p characteristic.
 */
bool pbl_pp_gatt_can_handle_characteristic(pbl_bt_characteristic_t characteristic);

/**
 * @brief Handle a subscription confirmation.
 *
 * @param characteristic Characteristic handle.
 * @param subscription_type Subscription now in effect.
 * @param error GATT error, if any.
 */
void pbl_pp_gatt_handle_subscribe(pbl_bt_characteristic_t characteristic,
                                  BLESubscription subscription_type, enum pbl_bt_gatt_error error);

/**
 * @brief Handle a read response or a notification.
 *
 * @param characteristic Characteristic handle.
 * @param value Value read or notified.
 * @param value_length Length of @p value in bytes.
 * @param error GATT error, if any.
 */
void pbl_pp_gatt_handle_read_or_notification(pbl_bt_characteristic_t characteristic,
                                             const uint8_t *value, size_t value_length,
                                             enum pbl_bt_gatt_error error);

/** @brief Clear the count of disconnections caused by repeated resets. */
void pbl_pp_gatt_reset_disconnect_counter(void);

/** @} */
