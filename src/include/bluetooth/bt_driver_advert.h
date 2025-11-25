/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <bluetooth/bluetooth_types.h>

bool bt_driver_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms);

void bt_driver_advert_advertising_disable(void);

bool bt_driver_advert_client_get_tx_power(int8_t *tx_power);

void bt_driver_advert_set_advertising_data(const BLEAdData *ad_data);
