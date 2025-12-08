/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

int gh3x2x_tuning_service_init(void);
void gh3x2x_ble_notify(const uint8_t* p_data, uint32_t data_len);