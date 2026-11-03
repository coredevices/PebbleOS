/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/services/blob_db/notif_db.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/notifications/notification_storage.h>
#include <pbl/util/uuid.h>

#include <clar.h>

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_kernel_services_notifications.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>

// Stubs
////////////////////////////////////////////////////////////////

// The phone announces 64-bit times unless a test says otherwise
static bool s_phone_time64;

bool comm_session_has_capability(CommSession *session, CommSessionCapability capability) {
  return (capability == CommSessionTime64Support) ? s_phone_time64 : true;
}

void test_notif_db__initialize(void) {
  s_phone_time64 = true;
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  notification_storage_reset();
}

void test_notif_db__cleanup(void) {
}

void test_notif_db__get_length(void) {
  SerializedTimelineItemHeader hdr = {
    .common = {
      .ancs_uid = 1,
      .layout = 0,
      .flags = 0,
      .timestamp = 0,
    },
  };
  uuid_generate(&hdr.common.id);
  cl_assert_equal_i(notif_db_insert((uint8_t *)&hdr, UUID_SIZE, (uint8_t *)&hdr, sizeof(hdr)), 0);
  cl_assert_equal_i(notif_db_get_len((uint8_t *)&hdr, UUID_SIZE), sizeof(hdr));
}

void test_notif_db__insert_remove(void) {
  SerializedTimelineItemHeader hdr = {
    .common = {
      .ancs_uid = 1,
      .layout = 0,
      .flags = 0,
      .timestamp = 0,
    },
  };
  uuid_generate(&hdr.common.id);
  cl_assert_equal_i(notif_db_insert((uint8_t *)&hdr, UUID_SIZE, (uint8_t *)&hdr, sizeof(hdr)), 0);
  cl_assert_equal_i(notif_db_delete((uint8_t *)&hdr, UUID_SIZE), 0);
  cl_assert_equal_i(notif_db_get_len((uint8_t *)&hdr, UUID_SIZE), 0);
}

void test_notif_db__flush(void) {
  SerializedTimelineItemHeader hdr1 = {
    .common = {
      .ancs_uid = 1,
      .layout = 0,
      .flags = 0,
      .timestamp = 0,
    },
  };
  uuid_generate(&hdr1.common.id);
  SerializedTimelineItemHeader hdr2 = {
    .common = {
      .ancs_uid = 1,
      .layout = 0,
      .flags = 0,
      .timestamp = 0,
    },
  };
  uuid_generate(&hdr2.common.id);
  SerializedTimelineItemHeader hdr3 = {
    .common = {
      .ancs_uid = 1,
      .layout = 0,
      .flags = 0,
      .timestamp = 0,
    },
  };
  uuid_generate(&hdr3.common.id);

  cl_assert_equal_i(notif_db_insert((uint8_t *)&hdr1, UUID_SIZE, (uint8_t *)&hdr1, sizeof(hdr1)),
                    0);
  cl_assert_equal_i(notif_db_insert((uint8_t *)&hdr2, UUID_SIZE, (uint8_t *)&hdr2, sizeof(hdr2)),
                    0);
  cl_assert_equal_i(notif_db_insert((uint8_t *)&hdr3, UUID_SIZE, (uint8_t *)&hdr3, sizeof(hdr3)),
                    0);
  cl_assert_equal_i(notif_db_flush(), 0);
  fake_system_task_callbacks_invoke_pending();
  cl_assert_equal_i(notif_db_get_len((uint8_t *)&hdr1, UUID_SIZE), 0);
  cl_assert_equal_i(notif_db_get_len((uint8_t *)&hdr2, UUID_SIZE), 0);
  cl_assert_equal_i(notif_db_get_len((uint8_t *)&hdr3, UUID_SIZE), 0);
}

void test_notif_db__insert_32_bit_from_phone(void) {
  // A phone without 64-bit times sends the 32-bit header; this one past 2038
  s_phone_time64 = false;
  AttributeList attr_list = {};
  attribute_list_add_cstring(&attr_list, AttributeIdTitle, "Hello");
  TimelineItem *src = timeline_item_create_with_attributes(
      3000000000LL, 0, TimelineItemTypeNotification, LayoutIdNotification, &attr_list, nullptr);
  cl_assert(src);
  src->header.ancs_uid = 1;

  SerializedTimelineItemHeader hdr;
  timeline_item_serialize_header(src, &hdr);
  const size_t len = sizeof(hdr) + hdr.payload_length;
  uint8_t buf[len];
  memcpy(buf, &hdr, sizeof(hdr));
  timeline_item_serialize_payload(src, buf + sizeof(hdr), hdr.payload_length);
  const size_t v1_len = timeline_item_serialized_narrow(buf, len);

  cl_assert_equal_i(notif_db_insert((uint8_t *)&src->header.id, UUID_SIZE, buf, v1_len), 0);

  TimelineItem stored;
  cl_assert(notification_storage_get(&src->header.id, &stored));
  cl_assert(stored.header.timestamp == 3000000000LL);
  cl_assert_equal_i(stored.header.ancs_uid, 1);
  cl_assert_equal_s(attribute_get_string(&stored.attr_list, AttributeIdTitle, ""), "Hello");
  timeline_item_free_allocated_buffer(&stored);
  timeline_item_destroy(src);
  attribute_list_destroy_list(&attr_list);
}
