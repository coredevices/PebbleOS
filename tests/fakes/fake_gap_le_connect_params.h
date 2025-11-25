/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/gap_le_connect_params.h"

void fake_gap_le_connect_params_init(void);

ResponseTimeState fake_gap_le_connect_params_get_last_requested(void);

void fake_gap_le_connect_params_reset_last_requested(void);

void fake_gap_le_connect_params_set_actual_state(ResponseTimeState actual_state);
