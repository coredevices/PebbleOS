/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_advert Advertising
 * @ingroup bluetooth
 * @brief LE advertising.
 *
 * Advertising is connectable and general discoverable. Connections are only established through
 * it, so the GAP events of a connection (see @ref bluetooth_gap_le_connect) are reported through
 * the advertising event handler.
 *
 * The advertising data must be set before enabling advertising, and again after the host
 * re-synchronizes (see pbl_bt_handle_host_resynced()):
 *
 * @code{.c}
 * static const uint8_t flags[] = {
 *   2, 0x01, PBL_BT_AD_FLAGS_GEN_DISCOVERABLE_MASK | PBL_BT_AD_FLAGS_BR_EDR_NOT_SUPPORTED_MASK,
 * };
 * uint8_t buf[sizeof(struct pbl_bt_ad_data) + 2 * PBL_BT_AD_REPORT_DATA_MAX_LENGTH];
 * struct pbl_bt_ad_data *ad = (struct pbl_bt_ad_data *)buf;
 *
 * ad->ad_data_length = sizeof(flags);
 * ad->scan_resp_data_length = 0;
 * memcpy(ad->data, flags, sizeof(flags));
 *
 * if (pbl_bt_advert_set_advertising_data(ad)) {
 *   pbl_bt_advert_advertising_enable(100, 150);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Start advertising.
 *
 * Advertises until pbl_bt_advert_advertising_disable() is called or a connection is established.
 *
 * @param min_interval_ms Minimum advertising interval in milliseconds.
 * @param max_interval_ms Maximum advertising interval in milliseconds.
 * @return true if advertising started.
 */
bool pbl_bt_advert_advertising_enable(uint32_t min_interval_ms, uint32_t max_interval_ms);

/** @brief Stop advertising, if active. */
void pbl_bt_advert_advertising_disable(void);

/**
 * @brief Get the advertising transmit power.
 *
 * Not supported by the NimBLE backend, which always returns false.
 *
 * @param[out] tx_power Transmit power in dBm.
 * @return true if @p tx_power was set.
 */
bool pbl_bt_advert_client_get_tx_power(int8_t *tx_power);

/**
 * @brief Set the advertising data and scan response data.
 *
 * @param ad_data Advertising and scan response data, each at most
 *                PBL_BT_AD_REPORT_DATA_MAX_LENGTH bytes. Copied during the call.
 * @return true on success.
 */
bool pbl_bt_advert_set_advertising_data(const struct pbl_bt_ad_data *ad_data);

/** @} */
