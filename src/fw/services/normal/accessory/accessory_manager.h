/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum {
  AccessoryInputStateIdle,
  AccessoryInputStateMic,
  AccessoryInputStateSmartstrap
} AccessoryInputState;

void accessory_manager_init(void);
bool accessory_manager_set_state(AccessoryInputState state);
