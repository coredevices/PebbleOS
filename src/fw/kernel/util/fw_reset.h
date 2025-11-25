/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

void fw_prepare_for_reset(bool skip_bt_teardown);

void fw_reset_into_prf(void);

typedef enum RemoteResetType {
  RemoteResetRegular = 0x00,
  RemoteResetPrf = 0xff,
} RemoteResetType;
