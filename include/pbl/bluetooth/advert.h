/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <pbl/bluetooth/types.h>

bool pbl_bt_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms);

void pbl_bt_advert_advertising_disable(void);

bool pbl_bt_advert_client_get_tx_power(int8_t *tx_power);

bool pbl_bt_advert_set_advertising_data(const struct pbl_bt_ad_data *ad_data);
