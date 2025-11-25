/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void fake_shared_prf_storage_reset_counts(void);
int fake_shared_prf_storage_get_ble_store_count(void);
int fake_shared_prf_storage_get_ble_delete_count(void);
int fake_shared_prf_storage_get_bt_classic_store_count(void);
int fake_shared_prf_storage_get_bt_classic_platform_bits_count(void);
int fake_shared_prf_storage_get_bt_classic_delete_count(void);
