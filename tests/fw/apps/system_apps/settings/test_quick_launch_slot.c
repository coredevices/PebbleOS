/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <clar.h>

#include <apps/system/settings/quick_launch_slot.h>
#include <apps/system/timeline/timeline.h>

static QuickLaunchSlotType s_called_type;
static ButtonId s_called_button;
static SwipeDirection s_called_direction;
static AppInstallId s_app;
static bool s_enabled;

AppInstallId quick_launch_single_click_get_app(ButtonId button) {
  s_called_type = QuickLaunchSlotTap;
  s_called_button = button;
  return s_app;
}

AppInstallId quick_launch_get_app(ButtonId button) {
  s_called_type = QuickLaunchSlotHold;
  s_called_button = button;
  return s_app;
}

AppInstallId quick_launch_swipe_get_app(SwipeDirection direction) {
  s_called_type = QuickLaunchSlotSwipe;
  s_called_direction = direction;
  return s_app;
}

void quick_launch_single_click_set_app(ButtonId button, AppInstallId app_id) {
  s_called_type = QuickLaunchSlotTap;
  s_called_button = button;
  s_app = app_id;
}

void quick_launch_set_app(ButtonId button, AppInstallId app_id) {
  s_called_type = QuickLaunchSlotHold;
  s_called_button = button;
  s_app = app_id;
}

void quick_launch_swipe_set_app(SwipeDirection direction, AppInstallId app_id) {
  s_called_type = QuickLaunchSlotSwipe;
  s_called_direction = direction;
  s_app = app_id;
}

void quick_launch_single_click_set_enabled(ButtonId button, bool enabled) {
  s_called_type = QuickLaunchSlotTap;
  s_called_button = button;
  s_enabled = enabled;
}

void quick_launch_set_enabled(ButtonId button, bool enabled) {
  s_called_type = QuickLaunchSlotHold;
  s_called_button = button;
  s_enabled = enabled;
}

void quick_launch_swipe_set_enabled(SwipeDirection direction, bool enabled) {
  s_called_type = QuickLaunchSlotSwipe;
  s_called_direction = direction;
  s_enabled = enabled;
}

void test_quick_launch_slot__initialize(void) {
  s_app = INSTALL_ID_INVALID;
  s_enabled = true;
}

void test_quick_launch_slot__dispatches_get_set_and_clear(void) {
  const QuickLaunchSlot slots[] = {
    {.type = QuickLaunchSlotTap, .button = BUTTON_ID_UP},
    {.type = QuickLaunchSlotTap, .button = BUTTON_ID_DOWN},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_UP},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_SELECT},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_DOWN},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_BACK},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Up},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Down},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Left},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Right},
  };
  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
    const QuickLaunchSlot slot = slots[i];
    quick_launch_slot_set_app(slot, 19);
    cl_assert_equal_i(s_called_type, slot.type);
    cl_assert_equal_i(s_app, 19);
    if (slot.type == QuickLaunchSlotSwipe) {
      cl_assert_equal_i(s_called_direction, slot.direction);
    } else {
      cl_assert_equal_i(s_called_button, slot.button);
    }
    cl_assert_equal_i(quick_launch_slot_get_app(slot), 19);
    cl_assert_equal_i(s_called_type, slot.type);
    if (slot.type == QuickLaunchSlotSwipe) {
      cl_assert_equal_i(s_called_direction, slot.direction);
    } else {
      cl_assert_equal_i(s_called_button, slot.button);
    }
    quick_launch_slot_clear(slot);
    cl_assert_equal_i(s_app, INSTALL_ID_INVALID);
    cl_assert_equal_i(s_enabled, false);
    cl_assert_equal_i(s_called_type, slot.type);
    if (slot.type == QuickLaunchSlotSwipe) {
      cl_assert_equal_i(s_called_direction, slot.direction);
    } else {
      cl_assert_equal_i(s_called_button, slot.button);
    }
    s_enabled = true;
  }
}

void test_quick_launch_slot__timeline_filter(void) {
  const Uuid future = TIMELINE_UUID_INIT;
  const Uuid past = TIMELINE_PAST_UUID_INIT;
  const Uuid full = TIMELINE_FULL_UUID_INIT;
  const Uuid other = {0};
  const QuickLaunchSlot slots[] = {
    {.type = QuickLaunchSlotTap, .button = BUTTON_ID_UP},
    {.type = QuickLaunchSlotTap, .button = BUTTON_ID_DOWN},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_UP},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_SELECT},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_DOWN},
    {.type = QuickLaunchSlotHold, .button = BUTTON_ID_BACK},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Up},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Down},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Left},
    {.type = QuickLaunchSlotSwipe, .direction = SwipeDirection_Right},
  };
  const bool expected[][3] = {
    {false, true, false}, {true, false, false},
    {true, true, true}, {true, true, true}, {true, true, true}, {true, true, true},
    {true, false, false}, {false, true, false},
    {true, true, false}, {true, true, false},
  };
  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
    cl_assert_equal_i(quick_launch_slot_allows_app(slots[i], &future), expected[i][0]);
    cl_assert_equal_i(quick_launch_slot_allows_app(slots[i], &past), expected[i][1]);
    cl_assert_equal_i(quick_launch_slot_allows_app(slots[i], &full), expected[i][2]);
    cl_assert(quick_launch_slot_allows_app(slots[i], &other));
  }
}
