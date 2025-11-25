/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/notifications/ancs/ancs_notifications.h"
#include "services/normal/notifications/ancs/ancs_notifications_util.h"
#include "services/normal/notifications/ancs/ancs_item.h"
#include "comm/ble/kernel_le_client/ancs/ancs_util.h"
#include "comm/ble/kernel_le_client/ancs/ancs_types.h"


// Test Data
///////////////////////////////////////////////////////////
#include "test_data.h"


// Stubs
///////////////////////////////////////////////////////////
#include "stubs_common.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_prompt.h"
#include "stubs_sleep.h"
#include "stubs_nexmo.h"
#include "stubs_codepoint.h"
#include "stubs_utf8.h"


// Externs
///////////////////////////////////////////////////////////
extern const int TIMELINE_ACTION_ENDPOINT;


// Fakes / Helpers
///////////////////////////////////////////////////////////
#include "fake_spi_flash.h"

static const uint8_t *s_expected_send_data = NULL;
static bool s_sent_action = false;

bool comm_session_send_data(CommSession *session, uint16_t endpoint_id,
                            const uint8_t* data, size_t length, uint32_t timeout_ms) {
  if (s_expected_send_data == NULL) {
    return false;
  }

  if (endpoint_id != TIMELINE_ACTION_ENDPOINT) {
    return false;
  }

  cl_assert_equal_m(s_expected_send_data, data, length);
  s_sent_action = true;
  return true;
}

static void prv_support_sms_replies(void) {
  // Store the preference in our DB
  char *key = "com.apple.MobileSMS";
  int key_len = strlen(key);
  const int flags = 0;
  const int num_attributes = 0;
  const int num_actions = 1;
  const int action_id = 12;
  const int reply_action_attrs = 2; // Title + Emoji

  uint8_t val[] = {0, 0, 0, 0, // Flags (unused for now)
                   num_attributes, num_actions, action_id,
                   TimelineItemActionTypeAncsResponse, // 0x0D
                   reply_action_attrs,
                   AttributeIdTitle, 0x05, 0x00, // Length
                   0x52, 0x65, 0x70, 0x6c, 0x79, // "Reply"
                   AttributeIdEmojiSupported, 0x01, 0x00, // Length
                   0x01, // "Supported"
                  };
  int val_len = sizeof(val);
  ios_notif_pref_db_insert((uint8_t *) key, key_len, (void *) &val, val_len);
}

static void prv_support_sms_replies_no_emoji(void) {
  // Store the preference in our DB
  char *key = "com.apple.MobileSMS";
  int key_len = strlen(key);
  const int flags = 0;
  const int num_attributes = 0;
  const int num_actions = 1;
  const int action_id = 12;
  const int reply_action_attrs = 2; // Title + Emoji

  uint8_t val[] = {0, 0, 0, 0, // Flags (unused for now)
                   num_attributes, num_actions, action_id,
                   TimelineItemActionTypeAncsResponse, // 0x0D
                   reply_action_attrs,
                   AttributeIdTitle, 0x05, 0x00, // Length
                   0x52, 0x65, 0x70, 0x6c, 0x79, // "Reply"
                   AttributeIdEmojiSupported, 0x01, 0x00, // Length
                   0x00, // "Not supported"
                  };
  int val_len = sizeof(val);
  ios_notif_pref_db_insert((uint8_t *) key, key_len, (void *) &val, val_len);
}


// Tests
///////////////////////////////////////////////////////////
void test_ancs_pebble_actions__initialize(void) {
  s_expected_send_data = NULL;
  s_sent_action = false;

  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
}

void test_ancs_pebble_actions__cleanup(void) {
}


void test_ancs_pebble_actions__test_sms_reply(void) {
  prv_support_sms_replies();

  ANCSAttribute *notif_attributes[NUM_FETCHED_NOTIF_ATTRIBUTES] = {0};
  ANCSAttribute *app_attrs[NUM_FETCHED_APP_ATTRIBUTES] = {0};

  const size_t header_len = sizeof(GetNotificationAttributesMsg);
  bool error = false;
  const bool complete = ancs_util_get_attr_ptrs(s_sms_ancs_data + header_len,
                                                ARRAY_LENGTH(s_sms_ancs_data) - header_len,
                                                s_fetched_notif_attributes,
                                                NUM_FETCHED_NOTIF_ATTRIBUTES,
                                                notif_attributes,
                                                &error);

  cl_assert(complete);
  cl_assert(!error);

  time_t timestamp = 0;
  ANCSAppMetadata app_metadata = {0};
  const ANCSAttribute *app_id = notif_attributes[FetchedNotifAttributeIndexAppID];
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs(app_id->value, app_id->length);
  TimelineItem *notif = ancs_item_create_and_populate(notif_attributes,
                                                      app_attrs,
                                                      &app_metadata,
                                                      notif_prefs,
                                                      timestamp,
                                                      ANCSProperty_None);

  cl_assert(notif);
  cl_assert_equal_i(notif->action_group.num_actions, 2);
  cl_assert_equal_i(notif->action_group.actions[0].type, TimelineItemActionTypeAncsNegative);
  TimelineItemAction *response_action = &notif->action_group.actions[1];
  cl_assert_equal_i(response_action->type, TimelineItemActionTypeAncsResponse);
  cl_assert_equal_i(response_action->attr_list.num_attributes, 2);
  cl_assert_equal_i(response_action->attr_list.attributes[0].id, AttributeIdTitle);
  cl_assert_equal_s(response_action->attr_list.attributes[0].cstring, "Reply");
  cl_assert_equal_i(response_action->attr_list.attributes[1].id, AttributeIdEmojiSupported);
  cl_assert_equal_i(response_action->attr_list.attributes[1].uint8, 1);

  ActionMenuItem menu_item = (ActionMenuItem) {
    .action_data = &notif->action_group.actions[1],
  };
  s_expected_send_data = s_sms_action_data;
  timeline_actions_invoke_action(&notif->action_group.actions[1], notif, NULL /*complete_cb*/,
                                 NULL /*cb_data*/);
  cl_assert(s_sent_action);
}

void test_ancs_pebble_actions__test_sms_reply_no_emoji(void) {
  prv_support_sms_replies_no_emoji();

  ANCSAttribute *notif_attributes[NUM_FETCHED_NOTIF_ATTRIBUTES] = {0};
  ANCSAttribute *app_attrs[NUM_FETCHED_APP_ATTRIBUTES] = {0};

  const size_t header_len = sizeof(GetNotificationAttributesMsg);
  bool error = false;
  const bool complete = ancs_util_get_attr_ptrs(s_sms_ancs_data + header_len,
                                                ARRAY_LENGTH(s_sms_ancs_data) - header_len,
                                                s_fetched_notif_attributes,
                                                NUM_FETCHED_NOTIF_ATTRIBUTES,
                                                notif_attributes,
                                                &error);

  cl_assert(complete);
  cl_assert(!error);

  time_t timestamp = 0;
  ANCSAppMetadata app_metadata = {0};
  const ANCSAttribute *app_id = notif_attributes[FetchedNotifAttributeIndexAppID];
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs(app_id->value, app_id->length);
  TimelineItem *notif = ancs_item_create_and_populate(notif_attributes,
                                                     app_attrs,
                                                     &app_metadata,
                                                     notif_prefs,
                                                     timestamp,
                                                     ANCSProperty_None);

  cl_assert(notif);
  cl_assert_equal_i(notif->action_group.num_actions, 2);
  cl_assert_equal_i(notif->action_group.actions[0].type, TimelineItemActionTypeAncsNegative);
  TimelineItemAction *response_action = &notif->action_group.actions[1];
  cl_assert_equal_i(response_action->type, TimelineItemActionTypeAncsResponse);
  cl_assert_equal_i(response_action->attr_list.num_attributes, 2);
  cl_assert_equal_i(response_action->attr_list.attributes[0].id, AttributeIdTitle);
  cl_assert_equal_s(response_action->attr_list.attributes[0].cstring, "Reply");
  cl_assert_equal_i(response_action->attr_list.attributes[1].id, AttributeIdEmojiSupported);
  cl_assert_equal_i(response_action->attr_list.attributes[1].uint8, 0);


  s_expected_send_data = s_sms_action_data;
  timeline_actions_invoke_action(&notif->action_group.actions[1], notif, NULL /*complete_cb*/,
                                 NULL /*cb_data*/);
  cl_assert(s_sent_action);
}

void test_ancs_pebble_actions__test_sms_replies_unsupported(void) {
  prv_support_sms_replies();

  ANCSAttribute *notif_attributes[NUM_FETCHED_NOTIF_ATTRIBUTES] = {0};
  ANCSAttribute *app_attrs[NUM_FETCHED_APP_ATTRIBUTES] = {0};

  const size_t header_len = sizeof(GetNotificationAttributesMsg);
  bool error = false;
  const bool complete = ancs_util_get_attr_ptrs(s_sms_ancs_data + header_len,
                                                ARRAY_LENGTH(s_sms_ancs_data) - header_len,
                                                s_fetched_notif_attributes,
                                                NUM_FETCHED_NOTIF_ATTRIBUTES,
                                                notif_attributes,
                                                &error);

  cl_assert(complete);
  cl_assert(!error);

  time_t timestamp = 0;
  ANCSAppMetadata app_metadata = {0};

  TimelineItem *item = ancs_item_create_and_populate(notif_attributes,
                                                     app_attrs,
                                                     &app_metadata,
                                                     NULL,
                                                     timestamp,
                                                     ANCSProperty_None);

  cl_assert(item);
  cl_assert_equal_i(item->action_group.num_actions, 1);
  cl_assert_equal_i(item->action_group.actions[0].type, TimelineItemActionTypeAncsNegative);
}

void test_ancs_pebble_actions__test_group_sms(void) {
  prv_support_sms_replies();

  ANCSAttribute *notif_attributes[NUM_FETCHED_NOTIF_ATTRIBUTES] = {0};
  ANCSAttribute *app_attrs[NUM_FETCHED_APP_ATTRIBUTES] = {0};

  const size_t header_len = sizeof(GetNotificationAttributesMsg);
  bool error = false;
  const bool complete = ancs_util_get_attr_ptrs(s_group_sms_ancs_data + header_len,
                                                ARRAY_LENGTH(s_group_sms_ancs_data) - header_len,
                                                s_fetched_notif_attributes,
                                                NUM_FETCHED_NOTIF_ATTRIBUTES,
                                                notif_attributes,
                                                &error);

  cl_assert(complete);
  cl_assert(!error);

  time_t timestamp = 0;
  ANCSAppMetadata app_metadata = {0};
  const ANCSAttribute *app_id = notif_attributes[FetchedNotifAttributeIndexAppID];
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs(app_id->value, app_id->length);
  TimelineItem *item = ancs_item_create_and_populate(notif_attributes,
                                                     app_attrs,
                                                     &app_metadata,
                                                     notif_prefs,
                                                     timestamp,
                                                     ANCSProperty_None);

  cl_assert(item);

  // We no longer show the reply action on group SMS messages (a less confusing UX)
  // Before the phone would reject the message right away.
  cl_assert_equal_i(item->action_group.num_actions, 1);
  cl_assert_equal_i(item->action_group.actions[0].type, TimelineItemActionTypeAncsNegative);
}

// void test_ancs_pebble_actions__test_email(void) {
  // ANCSAttribute *notif_attributes[NUM_FETCHED_NOTIF_ATTRIBUTES] = {0};
  // ANCSAttribute *app_attrs[NUM_FETCHED_APP_ATTRIBUTES] = {0};

  // const size_t header_len = sizeof(GetNotificationAttributesMsg);
  // bool error = false;
  // const bool complete = ancs_util_get_attr_ptrs(s_email_ancs_data + header_len,
  //                                               ARRAY_LENGTH(s_email_ancs_data) - header_len,
  //                                               s_fetched_notif_attributes,
  //                                               NUM_FETCHED_NOTIF_ATTRIBUTES,
  //                                               notif_attributes,
  //                                               &error);

  // cl_assert(complete);
  // cl_assert(!error);

  // time_t timestamp = 0;
  // ANCSAppMetadata app_metadata = {0};
  // TimelineItem *item = ancs_item_create_and_populate(notif_attributes,
  //                                                    app_attrs,
  //                                                    &app_metadata,
  //                                                    timestamp,
  //                                                    ANCSProperty_None);

  // cl_assert_equal_i(item->action_group.num_actions, 1);
  // cl_assert_equal_i(item->action_group.actions[0].type, TimelineItemActionTypeAncsNegative);

  // No e-mail actions yet

  // TimelineItemAction *response_action = &item->action_group.actions[1];
  // cl_assert_equal_i(response_action->type, TimelineItemActionTypeAncsResponse);
  // cl_assert_equal_i(response_action->attr_list.num_attributes, 3);
  // cl_assert_equal_i(response_action->attr_list.attributes[0].id, AttributeIdTitle);
  // cl_assert_equal_s(response_action->attr_list.attributes[0].cstring, "Reply");
  // cl_assert_equal_i(response_action->attr_list.attributes[1].id, AttributeIdSender);
  // cl_assert_equal_s(response_action->attr_list.attributes[1].cstring, "Philip G");
  // cl_assert_equal_i(response_action->attr_list.attributes[2].id, AttributeIdiOSAppIdentifier);
  // cl_assert_equal_s(response_action->attr_list.attributes[2].cstring, "com.apple.MobileSMS");


  // s_expected_send_data = s_email_action_data;
  // timeline_invoke_action(item, response_action, &response_action->attr_list);
  // cl_assert(s_sent_action);
// }
