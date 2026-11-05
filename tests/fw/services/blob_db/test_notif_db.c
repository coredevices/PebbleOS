/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/services/blob_db/notif_db.h>
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

void test_notif_db__initialize(void) {
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
