/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/timeline/timeline_actions.h"

// Test Data
///////////////////////////////////////////////////////////
#include "test_data.h"

static TimelineItemAction s_reply_action  = {
  .id = 0,
  .type = TimelineItemActionTypeResponse,
  .attr_list = (AttributeList) {
    .num_attributes = 1,
    .attributes = (Attribute[1]) {
      { .id = AttributeIdTitle, .cstring = "Reply" }
    }
  }
};

// Stubs
///////////////////////////////////////////////////////////
#include "stubs_common.h"


// Externs
///////////////////////////////////////////////////////////
extern const int TIMELINE_ACTION_ENDPOINT;

typedef struct ActionResultData ActionResultData;
extern ActionResultData *prv_invoke_action(ActionMenu *action_menu,
                                           const TimelineItemAction *action,
                                           const TimelineItem *pin, const char *label);


// Fakes / Helpers
///////////////////////////////////////////////////////////
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


// Setup
/////////////////////////
void test_timeline_actions__initialize(void) {
  s_expected_send_data = NULL;
  s_sent_action = false;
}

void test_timeline_actions__cleanup(void) {
}

// Tests
///////////////////////////

// Tests a regular response to a notification
void test_timeline_actions__response(void) {
  const TimelineItem item = {
    .attr_list = (AttributeList) {
      .num_attributes = 5,
      .attributes = (Attribute[5]) {
        { .id = AttributeIdTitle, .cstring = "Ian Graham" },
        { .id = AttributeIdBody, .cstring = "this is a test notification" },
        { .id = AttributeIdIconTiny, .uint32 = TIMELINE_RESOURCE_GENERIC_SMS },
        { .id = AttributeIdBgColor, .uint8 = GColorIslamicGreenARGB8 }
      }
    },
    .action_group = (TimelineItemActionGroup) {
      .num_actions = 1,
      .actions = &s_reply_action
    }
  };

  s_expected_send_data = s_sms_reply_action_data;
  prv_invoke_action(NULL, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}

// Tests that we send the required data for the Send Text app and reply to call features
void test_timeline_actions__send_text(void) {
  const TimelineItem item = {
    .header = {
      .id = UUID_SEND_SMS
    },
    .attr_list = (AttributeList) {
      .num_attributes = 2,
      .attributes = (Attribute[2]) {
        { .id = AttributeIdSender, .cstring = "555-123-4567" },
        { .id = AttributeIdiOSAppIdentifier, .cstring = "com.pebble.android.phone" }
      }
    },
    .action_group = (TimelineItemActionGroup) {
      .num_actions = 1,
      .actions = &s_reply_action
    }
  };

  s_expected_send_data = s_send_text_data;
  prv_invoke_action(NULL, &item.action_group.actions[0], &item, "Yo, what's up?");
  cl_assert(s_sent_action);
}
