/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/timeline/item.h"

#include "util/size.h"

#include "clar.h"

// Stubs
////////////////////////////////////
#include "fake_rtc.h"
#include "stubs_fonts.h"
#include "stubs_layout_layer.h"
#include "stubs_passert.h"
#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_rand_ptr.h"

static uint8_t s_payload_complete[] = {
  // Attribute 1
  0x01,                     // Attribute ID - Title
  0x11, 0x00,               // Attribute Length
  // Attribute text: "Test Notification"
  0x54, 0x65, 0x73, 0x74, 0x20, 0x4e, 0x6f, 0x74,  0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f,
  0x6e,

  // Attribute 2
  0x02,                     // Attribute ID - Subtitle
  0x08, 0x00,               // Attribute Length
  // Attribute text: "Subtitle"
  'S', 'u', 'b', 't', 'i', 't', 'l', 'e',

  // Attribute 3
  0x03,                     // Attribute ID - Body
  0x3f, 0x00,               // Attribute Length
  // Attribute text: "This is a test notification. Look at it and behold the awesome."
  0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20,  0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x6e,
  0x6f, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74,  0x69, 0x6f, 0x6e, 0x2e, 0x20, 0x4c, 0x6f, 0x6f,
  0x6b, 0x20, 0x61, 0x74, 0x20, 0x69, 0x74, 0x20,  0x61, 0x6e, 0x64, 0x20, 0x62, 0x65, 0x68, 0x6f,
  0x6c, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x61,  0x77, 0x65, 0x73, 0x6f, 0x6d, 0x65, 0x2e,

  // Action 1
  0x00,                     // Action ID
  0x02,                     // Action Type - Pebble Protocol
  0x01,                     // Number of action attributes
  // Action Attributes
  0x01,                     // Attribute ID - Title
  0x07, 0x00,               // Attribute Length
  // Attribute text:
  'D', 'i', 's', 'm', 'i', 's', 's',

  // Action 2
  0x01,                     // Action ID
  0x02,                     // Action Type - Pebble Protocol
  0x02,                     // Number of action attributes
  // Action Attributes
  0x01,                     // Attribute 1 ID - Title
  0x04, 0x00,               // Attribute 1 Length
  // Attribute text:
  'L', 'i', 'k', 'e',
  0x07,                     // Attribute 2 ID - ANCS UID
  0x01, 0x00,               // Attribute 2 Length
  // Attribute text: "Test"
  0x01
};

void test_timeline_item__initialize(void) {
}

void test_timeline_item__cleanup(void) {
}

static const uint8_t s_serialized_attribute_list[] = {
  0x01,                     // Attribute 1 ID - Title
  0x04, 0x00,               // Attribute 1 Length
  // Attribute text:
  'L', 'i', 'k', 'e',
  0x02,                     // Attribute 1 ID - Title
  0x03, 0x00,               // Attribute 1 Length
  // Attribute text:
  'e', 'y', 'e',
};

static const uint8_t s_invalid_serialized_attribute_list[] = {
  0x01,                     // Attribute 1 ID - Title
  0x04, 0x00,               // Attribute 1 Length
  // Attribute text:
  'L', 'i', 'k', 'e',
  0x08,                     // Attribute 2 ID - String list
  0x4e, 0x00,               // Attribute 2 length
  // Attribute content
  0x74, 0x65, 0x73, 0x74, 0x00, 0xd0, 0x94, 0xd0,  0xb0, 0x00, 0xd0, 0x9d, 0xd0, 0xb5, 0xd1, 0x82,
  0x00, 0xd0, 0x9e, 0xd0, 0x9a, 0x00, 0xd0, 0xa5,  0xd0, 0xb0, 0x2d, 0xd1, 0x85, 0xd0, 0xb0, 0x00,
  0xd0, 0xa1, 0xd0, 0xbf, 0xd0, 0xb0, 0xd1, 0x81,  0xd0, 0xb8, 0xd0, 0xb1, 0xd0, 0xbe, 0x00, 0xd0,
  0xa5, 0xd0, 0xbe, 0xd1, 0x80, 0xd0, 0xbe, 0xd1,  0x88, 0xd0, 0xbe, 0x00, 0xd0, 0x9e, 0xd1, 0x82,
  0xd0, 0xbb, 0xd0, 0xb8, 0xd1, 0x87, 0xd0, 0xbd,  0xd0, 0xbe, 0x00, 0xd0, 0xa1, 0xd0, 0xba, 0xd0,
  0xbe, 0xd1, 0x80, 0xd0, 0xbe, 0x20, 0xd0, 0xb1,  0xd1, 0x83, 0xd0, 0xb4, 0xd1,
};

void test_timeline_item__get_serialized_attributes_length(void) {
  const uint8_t *cursor = s_serialized_attribute_list;
  int32_t result = attribute_get_buffer_size_for_serialized_attributes(2,
      &cursor, s_serialized_attribute_list + sizeof(s_serialized_attribute_list));
  cl_assert(result == 9);

  cursor = s_invalid_serialized_attribute_list;
  result = attribute_get_buffer_size_for_serialized_attributes(3,
      &cursor,
      (uint8_t *)s_invalid_serialized_attribute_list + sizeof(s_invalid_serialized_attribute_list));
  cl_assert(result < 0);
}

void test_timeline_item__deserialize_payload(void) {
  size_t buf_size = 18 + 9 + 64 + 8 + 5 + 5;
  char *buffer;
  TimelineItem *item = timeline_item_create(3, 2, (uint8_t[]) {1, 2}, buf_size, (uint8_t **)&buffer);
  timeline_item_deserialize_payload(item, buffer, buf_size, s_payload_complete, sizeof(s_payload_complete));
  cl_assert_equal_i(item->attr_list.num_attributes, 3);
  cl_assert_equal_i(item->attr_list.attributes[0].id, AttributeIdTitle);
  cl_assert_equal_s(item->attr_list.attributes[0].cstring, "Test Notification");
  cl_assert_equal_i(item->attr_list.attributes[1].id, AttributeIdSubtitle);
  cl_assert_equal_s(item->attr_list.attributes[1].cstring, "Subtitle");
  cl_assert_equal_i(item->attr_list.attributes[2].id, AttributeIdBody);
  cl_assert_equal_s(item->attr_list.attributes[2].cstring, "This is a test notification. "
                                                                "Look at it and behold the awesome.");
  cl_assert_equal_i(item->action_group.num_actions, 2);
  cl_assert_equal_i(item->action_group.actions[0].id, 0);
  cl_assert_equal_i(item->action_group.actions[0].type, TimelineItemActionTypeGeneric);
  cl_assert_equal_i(item->action_group.actions[0].attr_list.num_attributes, 1);
  cl_assert_equal_i(item->action_group.actions[0].attr_list.attributes[0].id, AttributeIdTitle);
  cl_assert_equal_s(item->action_group.actions[0].attr_list.attributes[0].cstring, "Dismiss");
  cl_assert_equal_i(item->action_group.actions[1].id, 1);
  cl_assert_equal_i(item->action_group.actions[1].type, TimelineItemActionTypeGeneric);
  cl_assert_equal_i(item->action_group.actions[1].attr_list.num_attributes, 2);
  cl_assert_equal_i(item->action_group.actions[1].attr_list.attributes[0].id, AttributeIdTitle);
  cl_assert_equal_s(item->action_group.actions[1].attr_list.attributes[0].cstring, "Like");
  cl_assert_equal_i(item->action_group.actions[1].attr_list.attributes[1].id, AttributeIdAncsAction);
  cl_assert_equal_i(item->action_group.actions[1].attr_list.attributes[1].uint8, 1);
  timeline_item_destroy(item);
}

static Attribute action1_attributes[] = {
  {.id = AttributeIdTitle, .cstring = "Dismiss"},
};

static Attribute action2_attributes[] = {
  {.id = AttributeIdTitle, .cstring = "Like"},
  {.id = AttributeIdAncsAction, .int8 = 1}
};

static Attribute attributes[] = {
    {.id = AttributeIdTitle, .cstring = "Test Notification"},
    {.id = AttributeIdSubtitle, .cstring = "Subtitle"},
    {.id = AttributeIdBody, .cstring = "This is a test notification. "
        "Look at it and behold the awesome."},
};

static TimelineItemAction actions[] = {
    {.id = 0, .type = TimelineItemActionTypeGeneric, .attr_list = {.num_attributes = ARRAY_LENGTH(action1_attributes), .attributes = action1_attributes}},
    {.id = 1, .type = TimelineItemActionTypeGeneric, .attr_list = {.num_attributes = ARRAY_LENGTH(action2_attributes), .attributes = action2_attributes}},
};

void test_timeline_item__serialize_payload(void) {
  TimelineItem item = {
    .attr_list.num_attributes = ARRAY_LENGTH(attributes),
    .attr_list.attributes = attributes,
    .action_group.num_actions = ARRAY_LENGTH(actions),
    .action_group.actions = actions,
  };

  uint8_t *buffer = malloc(sizeof(s_payload_complete));
  timeline_item_serialize_payload(&item, buffer, sizeof(s_payload_complete));
  cl_assert(memcmp(buffer, s_payload_complete, sizeof(s_payload_complete)) == 0);
}

void test_timeline_item__string_list(void) {
  StringList *list = malloc(20);
  memset(list, 0, 20);
  // no data
  list->serialized_byte_length = 0;
  cl_assert_equal_i(0, string_list_count(list));

  list->serialized_byte_length = 3;
  // 4 empty strings
  cl_assert_equal_i(4, string_list_count(list));
  cl_assert_equal_s("", string_list_get_at(list, 0));
  cl_assert_equal_s("", string_list_get_at(list, 1));
  cl_assert_equal_s("", string_list_get_at(list, 2));
  cl_assert_equal_s("", string_list_get_at(list, 3));

  // non-null-terminated string is treated as one string - this is the standard case
  // please note that the string will only be terminated if there's another \0 following
  // when deserializing the data, the deserializer will append the needed \0
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = 'c'; // end of data
  list->data[3] = 'd';
  list->data[4] = '\0';
  cl_assert_equal_i(1, string_list_count(list));
  cl_assert_equal_s("abcd", string_list_get_at(list, 0));

  // 1 string (null terminated) => 2 strings, last is empty
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0'; // end of data
  list->data[3] = '\0';
  cl_assert_equal_i(2, string_list_count(list));
  cl_assert_equal_s("ab", string_list_get_at(list, 0));
  cl_assert_equal_s("", string_list_get_at(list, 1));

  // 2 strings (non-null terminated) - this is the standard case
  list->serialized_byte_length = 4;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = 'c'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(2, string_list_count(list));
  cl_assert_equal_s("ab", string_list_get_at(list, 0));
  cl_assert_equal_s("c", string_list_get_at(list, 1));

  // 3 strings (last two are is empty)
  list->serialized_byte_length = 4;
  list->data[0] = 'a';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = '\0'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(3, string_list_count(list));
  cl_assert_equal_s("ab", string_list_get_at(list, 0));
  cl_assert_equal_s("", string_list_get_at(list, 1));
  cl_assert_equal_s("", string_list_get_at(list, 2));
  cl_assert_equal_s(NULL, string_list_get_at(list, 3));

  // 4 strings (first and last two are empty)
  list->serialized_byte_length = 4;
  list->data[0] = '\0';
  list->data[1] = 'b';
  list->data[2] = '\0';
  list->data[3] = '\0'; // end of data
  list->data[4] = '\0';
  cl_assert_equal_i(4, string_list_count(list));
  cl_assert_equal_s("", string_list_get_at(list, 0));
  cl_assert_equal_s("b", string_list_get_at(list, 1));
  cl_assert_equal_s("", string_list_get_at(list, 2));
  cl_assert_equal_s("", string_list_get_at(list, 3));

  // 2 strings (last is not terminated and will fall through) will return 2 strings
  // when deserializing, the deserializer puts a \0 at the end
  // this case demonstrates the problem with incorrectly initialized data
  list->serialized_byte_length = 3;
  list->data[0] = 'a';
  list->data[1] = '\0';
  list->data[2] = 'b'; // end of data
  list->data[3] = 'c';
  list->data[4] = '\0';
  cl_assert_equal_i(2, string_list_count(list));
  cl_assert_equal_s("a", string_list_get_at(list, 0));
  cl_assert_equal_s("bc", string_list_get_at(list, 1));
}

static TimelineItemAction s_basic_action_list[] = {
  { .id = 0, .type = TimelineItemActionTypeGeneric },
  { .id = 1, .type = TimelineItemActionTypeHttp    },
  { .id = 2, .type = TimelineItemActionTypeOpenPin },
};

void test_timeline_item__find_action_with_id(void) {
  // Make sure we're resilient to NULL items
  cl_assert_equal_p(timeline_item_find_action_with_id(NULL, 0), NULL);

  // Make sure we can handle timeline items with no actions
  TimelineItem item = {};
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 0), NULL);

  // Make sure we actually find the items we're looking for
  item.action_group.num_actions = ARRAY_LENGTH(s_basic_action_list);
  item.action_group.actions = s_basic_action_list;
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 0), &s_basic_action_list[0]);
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 1), &s_basic_action_list[1]);
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 2), &s_basic_action_list[2]);
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 3), NULL);

  item.header.id = UUID_INVALID;
  cl_assert_equal_p(timeline_item_find_action_with_id(&item, 0), NULL);
}

void test_timeline_item__find_action_by_type(void) {
  // Make sure we're resilient to NULL items
  cl_assert_equal_p(timeline_item_find_action_by_type(NULL, TimelineItemActionTypeGeneric), NULL);

  // Make sure we can handle timeline items with no actions
  TimelineItem item = {};
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, TimelineItemActionTypeGeneric), NULL);

  // Make sure we actually find the items we're looking for
  item.action_group.num_actions = ARRAY_LENGTH(s_basic_action_list);
  item.action_group.actions = s_basic_action_list;
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, TimelineItemActionTypeGeneric),
                    &s_basic_action_list[0]);
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, TimelineItemActionTypeHttp),
                    &s_basic_action_list[1]);
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, TimelineItemActionTypeOpenPin),
                    &s_basic_action_list[2]);
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, TimelineItemActionTypeRemove), NULL);

  item.header.id = UUID_INVALID;
  cl_assert_equal_p(timeline_item_find_action_by_type(&item, 0), NULL);
}

void test_timeline_item__find_dismiss_action(void) {
  // Make sure we're resilient to NULL items
  cl_assert_equal_p(timeline_item_find_dismiss_action(NULL), NULL);

  // Make sure we can handle timeline items with no actions
  TimelineItem item = {};
  cl_assert_equal_p(timeline_item_find_dismiss_action(&item), NULL);

  // Copy the action list since it's easiest to just modify it
  TimelineItemAction *action_list = malloc(sizeof(s_basic_action_list));
  memcpy(action_list, s_basic_action_list, sizeof(s_basic_action_list));
  item.action_group.num_actions = ARRAY_LENGTH(s_basic_action_list);
  item.action_group.actions = action_list;

  // Make sure we don't return anything if the action doesn't exist
  cl_assert_equal_p(timeline_item_find_dismiss_action(&item), NULL);

  // Make sure we find both dismiss and ancs negative actions
  action_list[1].type = TimelineItemActionTypeDismiss;
  cl_assert_equal_p(timeline_item_find_dismiss_action(&item), &action_list[1]);

  action_list[1].type = TimelineItemActionTypeAncsNegative;
  cl_assert_equal_p(timeline_item_find_dismiss_action(&item), &action_list[1]);

  item.header.id = UUID_INVALID;
  cl_assert_equal_p(timeline_item_find_dismiss_action(&item), NULL);
  free(action_list);
}

void test_timeline_item__find_reply_action(void) {
  // Make sure we're resilient to NULL items
  cl_assert_equal_p(timeline_item_find_reply_action(NULL), NULL);

  // Make sure we can handle timeline items with no actions
  TimelineItem item = {};
  cl_assert_equal_p(timeline_item_find_reply_action(&item), NULL);

  // Copy the action list since it's easiest to just modify it
  TimelineItemAction *action_list = malloc(sizeof(s_basic_action_list));
  memcpy(action_list, s_basic_action_list, sizeof(s_basic_action_list));
  item.action_group.num_actions = ARRAY_LENGTH(s_basic_action_list);
  item.action_group.actions = action_list;

  // Make sure we don't return anything if the action doesn't exist
  cl_assert_equal_p(timeline_item_find_reply_action(&item), NULL);

  // Make sure we find both response and ancs response actions
  action_list[1].type = TimelineItemActionTypeResponse;
  cl_assert_equal_p(timeline_item_find_reply_action(&item), &action_list[1]);

  action_list[1].type = TimelineItemActionTypeAncsResponse;
  cl_assert_equal_p(timeline_item_find_reply_action(&item), &action_list[1]);

  item.header.id = UUID_INVALID;
  cl_assert_equal_p(timeline_item_find_reply_action(&item), NULL);
  free(action_list);
}
