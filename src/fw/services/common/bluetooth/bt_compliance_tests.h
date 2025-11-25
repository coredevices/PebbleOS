/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

void bt_test_start(void);
void bt_test_stop(void);

bool bt_test_bt_sig_rf_test_mode(void);
void bt_test_enter_hci_passthrough(void);
