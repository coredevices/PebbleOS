/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <shell/normal/quick_launch.h>

typedef enum {
  QuickLaunchSlotTap,
  QuickLaunchSlotHold,
  QuickLaunchSlotSwipe,
} QuickLaunchSlotType;

typedef struct {
  QuickLaunchSlotType type;
  ButtonId button;
  SwipeDirection direction;
} QuickLaunchSlot;

AppInstallId quick_launch_slot_get_app(QuickLaunchSlot slot);
void quick_launch_slot_set_app(QuickLaunchSlot slot, AppInstallId app_id);
void quick_launch_slot_clear(QuickLaunchSlot slot);
bool quick_launch_slot_allows_app(QuickLaunchSlot slot, const Uuid *uuid);
