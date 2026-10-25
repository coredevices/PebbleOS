/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "quick_launch_slot.h"

#include <apps/system/timeline/timeline.h>

AppInstallId quick_launch_slot_get_app(QuickLaunchSlot slot) {
  switch (slot.type) {
    case QuickLaunchSlotTap:
      return quick_launch_single_click_get_app(slot.button);
    case QuickLaunchSlotHold:
      return quick_launch_get_app(slot.button);
    case QuickLaunchSlotSwipe:
      return quick_launch_swipe_get_app(slot.direction);
  }
  return INSTALL_ID_INVALID;
}

void quick_launch_slot_set_app(QuickLaunchSlot slot, AppInstallId app_id) {
  switch (slot.type) {
    case QuickLaunchSlotTap:
      quick_launch_single_click_set_app(slot.button, app_id);
      break;
    case QuickLaunchSlotHold:
      quick_launch_set_app(slot.button, app_id);
      break;
    case QuickLaunchSlotSwipe:
      quick_launch_swipe_set_app(slot.direction, app_id);
      break;
  }
}

void quick_launch_slot_clear(QuickLaunchSlot slot) {
  quick_launch_slot_set_app(slot, INSTALL_ID_INVALID);
  switch (slot.type) {
    case QuickLaunchSlotTap:
      quick_launch_single_click_set_enabled(slot.button, false);
      break;
    case QuickLaunchSlotHold:
      quick_launch_set_enabled(slot.button, false);
      break;
    case QuickLaunchSlotSwipe:
      quick_launch_swipe_set_enabled(slot.direction, false);
      break;
  }
}

bool quick_launch_slot_allows_app(QuickLaunchSlot slot, const Uuid *uuid) {
  const Uuid future = TIMELINE_UUID_INIT;
  const Uuid past = TIMELINE_PAST_UUID_INIT;
  const Uuid full = TIMELINE_FULL_UUID_INIT;

  if (slot.type == QuickLaunchSlotHold) {
    return true;
  }
  if (slot.type == QuickLaunchSlotSwipe &&
      (slot.direction == SwipeDirection_Left || slot.direction == SwipeDirection_Right)) {
    return !uuid_equal(uuid, &full);
  }
  if ((slot.type == QuickLaunchSlotTap && slot.button == BUTTON_ID_UP) ||
      (slot.type == QuickLaunchSlotSwipe && slot.direction == SwipeDirection_Down)) {
    return !uuid_equal(uuid, &future) && !uuid_equal(uuid, &full);
  }
  if ((slot.type == QuickLaunchSlotTap && slot.button == BUTTON_ID_DOWN) ||
      (slot.type == QuickLaunchSlotSwipe && slot.direction == SwipeDirection_Up)) {
    return !uuid_equal(uuid, &past) && !uuid_equal(uuid, &full);
  }
  return true;
}
