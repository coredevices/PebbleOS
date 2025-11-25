/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/notifications/alerts.h"
#include "services/normal/notifications/alerts_private.h"

#include "clar.h"

#include "stdbool.h"

// Stubs
/////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_events.h"
#include "stubs_firmware_update.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_notification_storage.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_vibes.h"
#include "stubs_vibe_score_info.h"
#include "fake_rtc.h"


// Overrides
/////////////////////////////////
void do_not_disturb_init(void) {
  return;
}

void vibe_intensity_init(void) {
  return;
}

static bool s_low_power_active = false;

bool low_power_is_active(void) {
  return s_low_power_active;
}

static bool s_dnd_active = false;

bool do_not_disturb_is_active(void) {
  return s_dnd_active;
}

#define NOTIFICATION_VIBE_HOLDOFF_TICKS 3073024 // Just above 3 seconds
// Setup
/////////////////////////////////

void test_alerts__initialize(void) {
  s_dnd_active = false;
  s_low_power_active = false;
  alerts_set_mask(AlertMaskAllOn);
}

void test_alerts__cleanup(void) {
}

// Tests
/////////////////////////////////

void test_alerts__enabled(void) {
  alerts_set_mask(AlertMaskAllOn);
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  // Should allow all notifications to go through
  cl_assert(alerts_should_notify_for_type(AlertMobile));
  cl_assert(alerts_should_notify_for_type(AlertReminder));
  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));

  // Should not allow invalid Notifications though
  cl_assert(!alerts_should_notify_for_type(AlertInvalid));
}

void test_alerts__disabled(void) {
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  // Should not allow any notifications through
  alerts_set_mask(AlertMaskAllOff);

  cl_assert(!alerts_should_notify_for_type(AlertInvalid));
  cl_assert(!alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_notify_for_type(AlertPhoneCall));
}

void test_alerts__do_not_disturb(void) {
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  // We now allow notifications to come through in DND mode
  s_dnd_active = true;

  cl_assert(alerts_should_notify_for_type(AlertMobile));
  cl_assert(alerts_should_notify_for_type(AlertReminder));
  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));

  // Should not allow invalid Notifications though
  cl_assert(!alerts_should_notify_for_type(AlertInvalid));
}

void test_alerts__low_power(void) {
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  // Should not allow any notifications through while in low power
  s_low_power_active = true;

  cl_assert(!alerts_should_notify_for_type(AlertInvalid));
  cl_assert(!alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_notify_for_type(AlertPhoneCall));
}

void test_alerts__phone_calls_only(void) {
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  alerts_set_mask(AlertMaskPhoneCalls);

  // Should allow a phone call notification
  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));

  // Should not allow any other notifications through
  cl_assert(!alerts_should_notify_for_type(AlertInvalid));
  cl_assert(!alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_notify_for_type(AlertReminder));
}

void test_alerts__migration(void) {
  cl_assert(alerts_get_mask() == AlertMaskAllOn);

  alerts_set_mask(AlertMaskAllOnLegacy);

  cl_assert(alerts_get_mask() == AlertMaskAllOn);
}

void test_alerts__dnd_interruptions(void) {
  s_dnd_active = true;
  alerts_set_dnd_mask(AlertMaskAllOff);
  alerts_set_mask(AlertMaskAllOn);

  cl_assert(alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_vibrate_for_type(AlertMobile));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertMobile));

  cl_assert(alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_vibrate_for_type(AlertReminder));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertReminder));

  cl_assert(alerts_should_notify_for_type(AlertOther));
  cl_assert(!alerts_should_vibrate_for_type(AlertOther));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertOther));

  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));
  cl_assert(!alerts_should_vibrate_for_type(AlertPhoneCall));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertPhoneCall));

  s_dnd_active = true;
  alerts_set_dnd_mask(AlertMaskPhoneCalls);
  alerts_set_mask(AlertMaskAllOn);

  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));
  cl_assert(alerts_should_vibrate_for_type(AlertPhoneCall));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertPhoneCall));

  cl_assert(alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_vibrate_for_type(AlertMobile));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertMobile));

  cl_assert(alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_vibrate_for_type(AlertReminder));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertReminder));

  cl_assert(alerts_should_notify_for_type(AlertOther));
  cl_assert(!alerts_should_vibrate_for_type(AlertOther));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertOther));

  s_dnd_active = true;
  alerts_set_dnd_mask(AlertMaskAllOff);
  alerts_set_mask(AlertMaskAllOff);

  cl_assert(!alerts_should_notify_for_type(AlertPhoneCall));
  cl_assert(!alerts_should_vibrate_for_type(AlertPhoneCall));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertPhoneCall));

  cl_assert(!alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_vibrate_for_type(AlertMobile));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertMobile));

  cl_assert(!alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_vibrate_for_type(AlertReminder));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertReminder));

  cl_assert(!alerts_should_notify_for_type(AlertOther));
  cl_assert(!alerts_should_vibrate_for_type(AlertOther));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertOther));

  s_dnd_active = false;
  alerts_set_dnd_mask(AlertMaskAllOff);
  alerts_set_mask(AlertMaskPhoneCalls);

  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));
  cl_assert(alerts_should_vibrate_for_type(AlertPhoneCall));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertPhoneCall));

  cl_assert(!alerts_should_notify_for_type(AlertMobile));
  cl_assert(!alerts_should_vibrate_for_type(AlertMobile));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertMobile));

  cl_assert(!alerts_should_notify_for_type(AlertReminder));
  cl_assert(!alerts_should_vibrate_for_type(AlertReminder));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertReminder));

  cl_assert(!alerts_should_notify_for_type(AlertOther));
  cl_assert(!alerts_should_vibrate_for_type(AlertOther));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(!alerts_should_enable_backlight_for_type(AlertOther));

  s_dnd_active = false;
  alerts_set_mask(AlertMaskAllOn);
  alerts_set_dnd_mask(AlertMaskAllOff);

  cl_assert(alerts_should_notify_for_type(AlertPhoneCall));
  cl_assert(alerts_should_vibrate_for_type(AlertPhoneCall));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertPhoneCall));

  cl_assert(alerts_should_notify_for_type(AlertMobile));
  cl_assert(alerts_should_vibrate_for_type(AlertMobile));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertMobile));

  cl_assert(alerts_should_notify_for_type(AlertReminder));
  cl_assert(alerts_should_vibrate_for_type(AlertReminder));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertReminder));

  cl_assert(alerts_should_notify_for_type(AlertOther));
  cl_assert(alerts_should_vibrate_for_type(AlertOther));
  alerts_set_notification_vibe_timestamp();
  fake_rtc_set_ticks(rtc_get_ticks() + NOTIFICATION_VIBE_HOLDOFF_TICKS);
  cl_assert(alerts_should_enable_backlight_for_type(AlertOther));
}
