/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

typedef enum {
  AccessoryInputStateIdle,
  AccessoryInputStateMfg,
  AccessoryInputStateMic,
  AccessoryInputStateImaging,
} AccessoryInputState;

void accessory_manager_init(void);
bool accessory_manager_set_state(AccessoryInputState state);
