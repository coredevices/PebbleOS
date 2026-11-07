/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/pp/send.h>
#include <pbl/services/timeline/timeline_actions.h>

#include <clar.h>

// Test Data
///////////////////////////////////////////////////////////
#include "test_data.h"

static TimelineItemAction s_reply_action = {
  .id = 0,
  .type = TimelineItemActionTypeResponse,
  .attr_list = (AttributeList){
    .num_attributes = 1, .attributes = (Attribute[1]){{.id = AttributeIdTitle, .cstring = "Reply"}}
  }
};

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_common.h"

#include <fake_sys_work.h>

// Externs
///////////////////////////////////////////////////////////

typedef struct ActionResultData ActionResultData;
extern ActionResultData *prv_invoke_action(ActionMenu *action_menu,
                                           const TimelineItemAction *action,
                                           const TimelineItem *pin, const char *label);

// Fakes / Helpers
///////////////////////////////////////////////////////////
static const uint8_t *s_expected_send_data = nullptr;
static bool s_sent_action = false;
static size_t s_sent_length = 0;
static bool s_window_state_supported = false;
static int s_send_count;

#define MAX_LAUNCHER_CALLBACKS 4
static struct {
  void (*callback)(void *data);
  void *data;
} s_launcher_callbacks[MAX_LAUNCHER_CALLBACKS];
static int s_num_launcher_callbacks;

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
  cl_assert(s_num_launcher_callbacks < MAX_LAUNCHER_CALLBACKS);
  s_launcher_callbacks[s_num_launcher_callbacks].callback = callback;
  s_launcher_callbacks[s_num_launcher_callbacks].data = data;
  s_num_launcher_callbacks++;
}

static void prv_run_launcher_callback(void) {
  cl_assert(s_num_launcher_callbacks > 0);
  void (*callback)(void *data) = s_launcher_callbacks[0].callback;
  void *data = s_launcher_callbacks[0].data;
  s_num_launcher_callbacks--;
  memmove(&s_launcher_callbacks[0], &s_launcher_callbacks[1],
          s_num_launcher_callbacks * sizeof(s_launcher_callbacks[0]));
  callback(data);
}

static TimelineItemAction s_dismiss_action = {
  .id = 1,
  .type = TimelineItemActionTypeDismiss,
};

bool notification_storage_get(const Uuid *id, TimelineItem *item_out) {
  *item_out = (TimelineItem){
    .header =
        {
          .id = *id,
          .parent_id = UUID_NOTIFICATIONS_DATA_SOURCE,
          .type = TimelineItemTypeNotification,
        },
    .action_group = {.num_actions = 1, .actions = &s_dismiss_action},
  };
  return true;
}

bool pbl_pp_session_has_capability(const struct pbl_pp_session *session,
                                   enum pbl_pp_capability capability) {
  return s_window_state_supported && capability == PBL_PP_CAP_NOTIFICATION_WINDOW_STATE;
}

int pbl_pp_send(struct pbl_pp_session *session, uint16_t endpoint_id, const uint8_t *data,
                size_t length, uint32_t timeout_ms) {
  s_send_count++;
  if (s_expected_send_data == nullptr) {
    return -EAGAIN;
  }

  if (endpoint_id != PBL_PP_ENDPOINT_TIMELINE_ACTION) {
    return -EAGAIN;
  }

  cl_assert_equal_m(s_expected_send_data, data, length);
  s_sent_action = true;
  s_sent_length = length;
  return 0;
}

// Setup
/////////////////////////
void test_timeline_actions__initialize(void) {
  s_expected_send_data = nullptr;
  s_sent_action = false;
  s_sent_length = 0;
  s_window_state_supported = false;
  s_send_count = 0;
  s_num_launcher_callbacks = 0;
}

void test_timeline_actions__cleanup(void) {
  fake_sys_work_callbacks_cleanup();
}

// Tests
///////////////////////////

// Tests a regular response to a notification
void test_timeline_actions__response(void) {
  const TimelineItem item = {
    .attr_list =
        (AttributeList){
          .num_attributes = 5,
          .attributes =
              (Attribute[5]){
                {.id = AttributeIdTitle, .cstring = "Ian Graham"},
                {.id = AttributeIdBody, .cstring = "this is a test notification"},
                {.id = AttributeIdIconTiny, .uint32 = TIMELINE_RESOURCE_GENERIC_SMS},
                {.id = AttributeIdBgColor, .uint8 = GColorIslamicGreenARGB8}
              }
        },
    .action_group = (TimelineItemActionGroup){.num_actions = 1, .actions = &s_reply_action}
  };

  s_expected_send_data = s_sms_reply_action_data;
  prv_invoke_action(nullptr, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}

// Tests that we send the required data for the Send Text app and reply to call features
void test_timeline_actions__send_text(void) {
  const TimelineItem item = {
    .header = {.id = UUID_SEND_SMS},
    .attr_list =
        (AttributeList){
          .num_attributes = 2,
          .attributes =
              (Attribute[2]){
                {.id = AttributeIdSender, .cstring = "555-123-4567"},
                {.id = AttributeIdiOSAppIdentifier, .cstring = "com.pebble.android.phone"}
              }
        },
    .action_group = (TimelineItemActionGroup){.num_actions = 1, .actions = &s_reply_action}
  };

  s_expected_send_data = s_send_text_data;
  prv_invoke_action(nullptr, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}

void test_timeline_actions__displayed_item(void) {
  const Uuid id = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                   0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
  const uint8_t expected[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
  s_window_state_supported = true;
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(&id);
  cl_assert(s_sent_action);
  cl_assert_equal_i(s_sent_length, sizeof(expected));
}

void test_timeline_actions__nothing_displayed(void) {
  const uint8_t expected[] = {0x04, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  s_window_state_supported = true;
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(nullptr);
  cl_assert(s_sent_action);
  cl_assert_equal_i(s_sent_length, sizeof(expected));
}

void test_timeline_actions__displayed_item_needs_phone_support(void) {
  const uint8_t expected[] = {0x04};
  s_expected_send_data = expected;
  timeline_action_endpoint_send_displayed_item(nullptr);
  cl_assert(!s_sent_action);
}

// Dismiss all sends one dismiss at a time from the background work queue, never from KernelMain.
void test_timeline_actions__dismiss_all_sends_from_sys_work(void) {
  NotificationInfo notifications[3];
  for (unsigned int i = 0U; i < ARRAY_LENGTH(notifications); i++) {
    notifications[i] = (NotificationInfo){.type = NotificationMobile, .id = {.byte0 = i + 1U}};
  }

  timeline_actions_dismiss_all(notifications, ARRAY_LENGTH(notifications), nullptr, nullptr,
                               nullptr);

  for (unsigned int i = 0U; i < ARRAY_LENGTH(notifications); i++) {
    cl_assert_equal_i(s_send_count, i);
    cl_assert_equal_i(list_count(s_sys_work_callback_head), 2);
    cl_assert_equal_i(s_num_launcher_callbacks, 0);

    fake_sys_work_callbacks_invoke(1);
    cl_assert_equal_i(s_send_count, i + 1);

    fake_sys_work_callbacks_invoke(1);
    cl_assert_equal_i(s_num_launcher_callbacks, 1);
    prv_run_launcher_callback();
  }

  cl_assert_equal_i(s_send_count, ARRAY_LENGTH(notifications));
  cl_assert_equal_i(list_count(s_sys_work_callback_head), 0);
  cl_assert_equal_i(s_num_launcher_callbacks, 0);
}
