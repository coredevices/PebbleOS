/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/time.h>
#include <pbl/services/blob_db/ios_notif_pref_db.h>
#include <pbl/services/notifications/alerts_preferences.h>
#include <pbl/pp/protocol.h>
#include <pbl/pp/send.h>
#include <pbl/services/notifications/ancs/ancs_notifications.h>

#include <string.h>

#include <clar.h>

// Stubs
////////////////////////////////////////////////////////////////
#include <stubs_uuid.h>

iOSNotifPrefs *ios_notif_pref_db_get_prefs(const uint8_t *app_id, int length) {
  return nullptr;
}

void ios_notif_pref_db_free_prefs(iOSNotifPrefs *prefs) {
  return;
}

void ancs_filtering_record_app(iOSNotifPrefs **app_notif_prefs,
                               const struct pbl_bt_ancs_attr *app_id,
                               const struct pbl_bt_ancs_attr *display_name,
                               const struct pbl_bt_ancs_attr *title) {
}

bool ancs_filtering_is_muted(const iOSNotifPrefs *app_notif_prefs) {
  return false;
}

bool ancs_filtering_matches_rules(const iOSNotifPrefs *app_notif_prefs,
                                  const struct pbl_bt_ancs_attr *title,
                                  const struct pbl_bt_ancs_attr *subtitle,
                                  const struct pbl_bt_ancs_attr *body) {
  return false;
}

// What the phone was told, see prv_announce_to_phone()
static struct pbl_pp_session *s_system_session = (struct pbl_pp_session *)1;
static struct pbl_pp_send_buffer *s_send_buffer = (struct pbl_pp_send_buffer *)2;
static uint16_t s_sent_endpoint;
static uint8_t s_sent[300];
static size_t s_sent_len;
static int s_sent_count;

struct pbl_pp_session *pbl_pp_session_get_system(void) {
  return s_system_session;
}

struct pbl_pp_send_buffer *pbl_pp_send_buffer_begin(struct pbl_pp_session *session,
                                                    uint16_t endpoint_id, size_t length,
                                                    uint32_t timeout_ms) {
  if (!session) {
    return nullptr;
  }
  s_sent_endpoint = endpoint_id;
  s_sent_len = 0;
  return s_send_buffer;
}

bool pbl_pp_send_buffer_write(struct pbl_pp_send_buffer *sb, const uint8_t *data, size_t length) {
  memcpy(&s_sent[s_sent_len], data, length);
  s_sent_len += length;
  return true;
}

void pbl_pp_send_buffer_end(struct pbl_pp_send_buffer *sb) {
  s_sent_count++;
}

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_events.h>
#include <fake_notification_storage.h>

static time_t s_now;
time_t pbl_realtime(void) {
  return s_now;
}

uint64_t pbl_uptime_ticks(void) {
  return 0;
}

void test_ancs_notifications__initialize(void) {
  s_now = 1;
  fake_notification_storage_reset();
  fake_event_init();
  s_system_session = (struct pbl_pp_session *)1;
  s_sent_count = 0;
  s_sent_len = 0;
}

void test_ancs_notifications__cleanup(void) {
}

void test_ancs_notifications__handle_phone_call_message(void) {
  const uint8_t app_id[] = {
    0x00, 21,  0x00, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e',
    '.',  'm', 'o',  'b', 'i', 'l', 'e', 'p', 'h', 'o', 'n', 'e',
  };
  const uint8_t title[] = {
    0x01,
    23,
    0x00,
    // Add some formatting modifiers before and after. iOS 10 seems to be sending phone numbers with
    // them now.
    0xe2,
    0x80,
    0xad,
    '+',
    '1',
    ' ',
    '(',
    '5',
    '1',
    '0',
    ')',
    ' ',
    '4',
    '4',
    '4',
    '-',
    '3',
    '3',
    '3',
    '3',
    0xe2,
    0x80,
    0xac,
  };
  const uint8_t subtitle[] = {
    0x02, 8, 0x00, 'P', 'e', 'b', 'b', 'l', 'e', 'H', 'Q',
  };
  const uint8_t message[] = {
    0x03, 13, 0x00, 'I', 'n', 'c', 'o', 'm', 'i', 'n', 'g', ' ', 'C', 'a', 'l', 'l',
  };
  const uint8_t date[] = {
    0x05,
    0x00,
    0x00,
  };
  const uint8_t positive_action[] = {
    0x06, 6, 0x00, 'A', 'n', 's', 'w', 'e', 'r',
  };
  const uint8_t negative_action[] = {
    0x07, 7, 0x00, 'D', 'e', 'c', 'l', 'i', 'n', 'e',
  };

  const struct pbl_bt_ancs_attr *notif_attributes[] = {
    [ANCS_NOTIF_ATTR_APP_ID] = (struct pbl_bt_ancs_attr *)&app_id,
    [ANCS_NOTIF_ATTR_TITLE] = (struct pbl_bt_ancs_attr *)&title,
    [ANCS_NOTIF_ATTR_SUBTITLE] = (struct pbl_bt_ancs_attr *)&subtitle,
    [ANCS_NOTIF_ATTR_MESSAGE] = (struct pbl_bt_ancs_attr *)&message,
    [ANCS_NOTIF_ATTR_DATE] = (struct pbl_bt_ancs_attr *)&date,
    [ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL] = (struct pbl_bt_ancs_attr *)&positive_action,
    [ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL] = (struct pbl_bt_ancs_attr *)&negative_action,
  };

  const uint8_t app_display_name[] = {
    0x00, 5, 0x0, 'P', 'h', 'o', 'n', 'e',
  };

  const struct pbl_bt_ancs_attr *app_attributes[] = {
    [ANCS_APP_ATTR_DISPLAY_NAME] = (struct pbl_bt_ancs_attr *)&app_display_name,
  };

  const struct pbl_bt_ancs_notif notif = {
    .uid = 37,
    .category = PBL_BT_ANCS_CATEGORY_INCOMING_CALL,
  };

  ancs_notifications_handle_message(&notif, false, false, notif_attributes, app_attributes);

  // We just processed an incoming phone call event, there better be a phone event scheduled!
  PebbleEvent event = fake_event_get_last();

  cl_assert_equal_i(event.type, PEBBLE_PHONE_EVENT);
  cl_assert_equal_i(event.phone.type, PhoneEventType_Incoming);
  cl_assert_equal_i(event.phone.source, PhoneCallSource_ANCS_Legacy);
}

static bool s_recovered;

static void prv_send_social_message(uint32_t uid, uint8_t flags) {
  static const uint8_t app_id[] = {
    0x00, 17,  0x00, 'c', 'o', 'm', '.', 't', 'e', 's',
    't',  's', '.',  'F', 'a', 'k', 'e', 'A', 'p', 'p',
  };
  static const uint8_t title[] = {
    0x01, 5, 0x00, 'T', 'i', 't', 'l', 'e',
  };
  static const uint8_t subtitle[] = {
    0x02,
    0x00,
    0x00,
  };
  static const uint8_t message[] = {
    0x03, 5, 0x00, 'H', 'e', 'l', 'l', 'o',
  };
  static const uint8_t date[] = {
    0x05,
    0x00,
    0x00,
  };
  static const uint8_t positive_action[] = {
    0x06,
    0x00,
    0x00,
  };
  static const uint8_t negative_action[] = {
    0x07,
    0x00,
    0x00,
  };

  const struct pbl_bt_ancs_attr *notif_attributes[] = {
    [ANCS_NOTIF_ATTR_APP_ID] = (struct pbl_bt_ancs_attr *)&app_id,
    [ANCS_NOTIF_ATTR_TITLE] = (struct pbl_bt_ancs_attr *)&title,
    [ANCS_NOTIF_ATTR_SUBTITLE] = (struct pbl_bt_ancs_attr *)&subtitle,
    [ANCS_NOTIF_ATTR_MESSAGE] = (struct pbl_bt_ancs_attr *)&message,
    [ANCS_NOTIF_ATTR_DATE] = (struct pbl_bt_ancs_attr *)&date,
    [ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL] = (struct pbl_bt_ancs_attr *)&positive_action,
    [ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL] = (struct pbl_bt_ancs_attr *)&negative_action,
  };

  static const uint8_t app_display_name[] = {
    0x00, 7, 0x0, 'F', 'a', 'k', 'e', 'A', 'p', 'p',
  };

  const struct pbl_bt_ancs_attr *app_attributes[] = {
    [ANCS_APP_ATTR_DISPLAY_NAME] = (struct pbl_bt_ancs_attr *)&app_display_name,
  };

  const struct pbl_bt_ancs_notif notif = {
    .uid = uid,
    .flags = flags,
    .category = PBL_BT_ANCS_CATEGORY_SOCIAL,
  };

  ancs_notifications_handle_message(&notif, s_recovered, false, notif_attributes, app_attributes);
}

void test_ancs_notifications__tells_phone_app_id_of_new_notifications(void) {
  prv_send_social_message(40, 0);
  cl_assert_equal_i(s_sent_count, 1);
  cl_assert_equal_i(s_sent_endpoint, PBL_PP_ENDPOINT_ANCS_EVENT);
  const uint8_t expected[] = {0x01, 17, 'c', 'o', 'm', '.', 't', 'e', 's', 't', 's',
                              '.',  'F', 'a', 'k', 'e', 'A', 'p', 'p'};
  cl_assert_equal_i(s_sent_len, sizeof(expected));
  cl_assert_equal_m(s_sent, expected, sizeof(expected));

  // Not for notifications recovered after a reconnection: the phone app is catching up anyway
  s_recovered = true;
  prv_send_social_message(41, 0);
  s_recovered = false;
  cl_assert_equal_i(s_sent_count, 1);

  // Nor without a phone app connected
  s_system_session = nullptr;
  prv_send_social_message(42, 0);
  cl_assert_equal_i(s_sent_count, 1);
}

void test_ancs_notifications__silent_property_sets_silent_header(void) {
  // Force the "update existing" path so the stored item is captured by the fake storage
  fake_notification_storage_set_existing_ancs_notification(&(Uuid)UUID_SYSTEM, UINT32_MAX);

  // Without the silent flag the stored notification is not marked silent
  prv_send_social_message(37, 0);
  cl_assert_equal_i(fake_notification_storage_get_store_count(), 1);
  cl_assert_equal_i(fake_notification_storage_get_last_notification()->header.silent, 0);

  // With the silent flag the stored notification is marked silent
  prv_send_social_message(38, PBL_BT_ANCS_FLAG_SILENT);
  cl_assert_equal_i(fake_notification_storage_get_store_count(), 2);
  cl_assert_equal_i(fake_notification_storage_get_last_notification()->header.silent, 1);
  cl_assert_equal_i(fake_notification_storage_get_last_notification()->header.ancs_notif, 1);
}

void test_ancs_notifications__handle_phone_call_removed(void) {
  const uint32_t uid = 5;
  const struct pbl_bt_ancs_notif notif = {
    .uid = uid,
    .event = PBL_BT_ANCS_EVENT_REMOVED,
    .category = PBL_BT_ANCS_CATEGORY_INCOMING_CALL,
  };

  // Make sure we don't send any events when this isn't iOS9
  ancs_notifications_handle_notification_removed(&notif, false);
  cl_assert_equal_i(fake_event_get_count(), 0);

  // Make sure that we trigger an event when this is iOS9
  ancs_notifications_handle_notification_removed(&notif, true);
  PebbleEvent event = fake_event_get_last();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert_equal_i(event.type, PEBBLE_PHONE_EVENT);
  cl_assert_equal_i(event.phone.type, PhoneEventType_Hide);
  cl_assert_equal_i(event.phone.source, PhoneCallSource_ANCS);
  cl_assert_equal_i(event.phone.call_identifier, uid);

  // Make sure that having a matching notification in notif_db still results in a call hide event
  fake_notification_storage_set_existing_ancs_notification(&(Uuid)UUID_SYSTEM, uid);
  ancs_notifications_handle_notification_removed(&notif, true);
  event = fake_event_get_last();
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert_equal_i(event.type, PEBBLE_PHONE_EVENT);
  cl_assert_equal_i(event.phone.type, PhoneEventType_Hide);
  cl_assert_equal_i(event.phone.source, PhoneCallSource_ANCS);
  cl_assert_equal_i(event.phone.call_identifier, uid);
}
