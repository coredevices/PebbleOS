/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <string.h>

#include <pbl/fs/fs.h>
#include <pbl/services/blob_db/timeline_item_storage.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/util/units.h>

#include <clar.h>

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_session.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>
#include <stubs_pbl_malloc.h>

// Stubs
////////////////////////////////////////////////////////////////
const char *timeline_get_private_data_source(Uuid *parent_id) {
  return nullptr;
}

// Fixture
////////////////////////////////////////////////////////////////
#define FILE_NAME        "/items64"
#define LEGACY_FILE_NAME "/items"
#define MAX_SIZE         PBL_KIB(40)
#define MAX_AGE          (3 * PBL_SEC_PER_DAY)

// 2065, past what a signed 32-bit time holds
#define TIME_PAST_2038 ((time_t)3000000000LL)

static TimelineItemStorage s_storage;

void test_timeline_item_storage__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_pfs_mount();
  rtc_set_time(TIME_PAST_2038 - PBL_SEC_PER_DAY);
}

void test_timeline_item_storage__cleanup(void) {
  timeline_item_storage_deinit(&s_storage);
}

static TimelineItem prv_item(uint8_t n, time_t timestamp, bool from_watch) {
  return (TimelineItem){
    .header = {
      .id =
          {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
           n},
      .parent_id =
          {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
           0x01},
      .timestamp = timestamp,
      .duration = 30,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .from_watch = from_watch,
    },
  };
}

//! An item as earlier firmware stored it: 32-bit header, flags and status inverted
static size_t prv_serialize_legacy(const TimelineItem *src, uint8_t *buf) {
  TimelineItem item = *src;
  SerializedTimelineItemHeader hdr;
  timeline_item_serialize_header(&item, &hdr);
  memcpy(buf, &hdr, sizeof(hdr));
  const size_t len = timeline_item_serialized_narrow(buf, sizeof(hdr));
  buf[offsetof(CommonTimelineItemHeader, flags) - 4] ^= 0xff;
  buf[offsetof(CommonTimelineItemHeader, status) - 4] ^= 0xff;
  return len;
}

static void prv_write_legacy(SettingsFile *file, const TimelineItem *item, time_t modified,
                             bool synced) {
  uint8_t buf[sizeof(SerializedTimelineItemHeader)];
  const size_t len = prv_serialize_legacy(item, buf);
  cl_assert_equal_i(
      settings_file_set_with_timestamp(file, &item->header.id, sizeof(Uuid), buf, len, modified),
      S_SUCCESS);
  if (synced) {
    cl_assert_equal_i(settings_file_mark_synced(file, &item->header.id, sizeof(Uuid)), S_SUCCESS);
  }
}

static void prv_assert_item(const TimelineItem *expected) {
  SerializedTimelineItemHeader hdr = {};
  cl_assert_equal_i(timeline_item_storage_read(&s_storage, (const uint8_t *)&expected->header.id,
                                               sizeof(Uuid), (uint8_t *)&hdr, sizeof(hdr)),
                    S_SUCCESS);
  TimelineItem item = {};
  timeline_item_deserialize_header(&item, &hdr);
  cl_assert(item.header.timestamp == expected->header.timestamp);
  cl_assert_equal_i(item.header.duration, expected->header.duration);
  cl_assert_equal_i(item.header.layout, expected->header.layout);
  cl_assert_equal_i(item.header.type, expected->header.type);
  cl_assert_equal_b(item.header.from_watch, expected->header.from_watch);
  cl_assert_equal_i(item.header.status, expected->header.status);
}

typedef struct {
  Uuid id;
  bool dirty;
  time_t last_modified;
  int count;
} RecordInfo;

static bool prv_find_record_cb(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  RecordInfo *record = context;
  if (info->key_len == sizeof(Uuid) && info->val_len > 0) {
    record->count++;
    Uuid id;
    info->get_key(file, &id, sizeof(id));
    if (uuid_equal(&id, &record->id)) {
      record->dirty = info->dirty;
      record->last_modified = info->last_modified;
    }
  }
  return true;
}

static RecordInfo prv_record(const TimelineItem *item) {
  RecordInfo record = {.id = item->header.id};
  timeline_item_storage_each(&s_storage, prv_find_record_cb, &record);
  return record;
}

static bool prv_file_exists(const char *name) {
  struct pbl_fs_dirent entry;
  return pbl_fs_stat(name, &entry) == 0;
}

// Tests
////////////////////////////////////////////////////////////////

void test_timeline_item_storage__migrate_legacy_file(void) {
  const TimelineItem synced = prv_item(1, TIME_PAST_2038, false);
  const TimelineItem dirty = prv_item(2, TIME_PAST_2038 + 3600, true);
  SettingsFile legacy;
  cl_assert_equal_i(settings_file_open_growable(&legacy, LEGACY_FILE_NAME, MAX_SIZE, PBL_KIB(8)),
                    S_SUCCESS);
  prv_write_legacy(&legacy, &synced, 1000000000, true);
  prv_write_legacy(&legacy, &dirty, 1100000000, false);
  settings_file_close(&legacy);

  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);

  prv_assert_item(&synced);
  prv_assert_item(&dirty);
  // Sync state and modification times are kept
  RecordInfo record = prv_record(&synced);
  cl_assert_equal_i(record.count, 2);
  cl_assert(!record.dirty);
  cl_assert(record.last_modified == 1000000000);
  record = prv_record(&dirty);
  cl_assert(record.dirty);
  cl_assert(record.last_modified == 1100000000);

  // Earlier firmware never sees 64-bit items
  cl_assert(!prv_file_exists(LEGACY_FILE_NAME));
  cl_assert(prv_file_exists(FILE_NAME));

  // Nothing left to migrate at the next boot
  timeline_item_storage_deinit(&s_storage);
  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);
  prv_assert_item(&synced);
  prv_assert_item(&dirty);
}

void test_timeline_item_storage__legacy_file_wins(void) {
  // A migration cut short, or a downgrade that wrote to the old file: start over from it
  const TimelineItem stale = prv_item(1, TIME_PAST_2038, false);
  const TimelineItem current = prv_item(2, TIME_PAST_2038, false);
  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);
  SerializedTimelineItemHeader hdr;
  TimelineItem item = stale;
  timeline_item_serialize_header(&item, &hdr);
  cl_assert_equal_i(timeline_item_storage_insert(&s_storage, (uint8_t *)&stale.header.id,
                                                 sizeof(Uuid), (uint8_t *)&hdr, sizeof(hdr), true),
                    S_SUCCESS);
  timeline_item_storage_deinit(&s_storage);

  SettingsFile legacy;
  cl_assert_equal_i(settings_file_open_growable(&legacy, LEGACY_FILE_NAME, MAX_SIZE, PBL_KIB(8)),
                    S_SUCCESS);
  prv_write_legacy(&legacy, &current, 1000000000, true);
  settings_file_close(&legacy);

  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);
  prv_assert_item(&current);
  cl_assert_equal_i(prv_record(&current).count, 1);
  cl_assert(!prv_file_exists(LEGACY_FILE_NAME));
}

void test_timeline_item_storage__migrate_full_legacy_file(void) {
  // A growable file filled to its maximum: the wider items need more room than it allowed
  const int max_size = PBL_KIB(12);
  SettingsFile legacy;
  cl_assert_equal_i(settings_file_open_growable(&legacy, LEGACY_FILE_NAME, max_size, PBL_KIB(8)),
                    S_SUCCESS);
  int count = 0;
  for (;; count++) {
    TimelineItem item = prv_item(count, TIME_PAST_2038 + count, false);
    item.header.id.byte14 = count >> 8;
    uint8_t buf[sizeof(SerializedTimelineItemHeader)];
    const size_t len = prv_serialize_legacy(&item, buf);
    if (settings_file_set(&legacy, &item.header.id, sizeof(Uuid), buf, len) != S_SUCCESS) {
      break;
    }
  }
  cl_assert(count > 100);
  settings_file_close(&legacy);

  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, max_size, MAX_AGE);

  for (int i = 0; i < count; i++) {
    TimelineItem item = prv_item(i, TIME_PAST_2038 + i, false);
    item.header.id.byte14 = i >> 8;
    prv_assert_item(&item);
  }
  cl_assert_equal_i(prv_record(&(TimelineItem){}).count, count);
  cl_assert(!prv_file_exists(LEGACY_FILE_NAME));
}

static bool prv_insert(const TimelineItem *src, bool synced) {
  TimelineItem item = *src;
  SerializedTimelineItemHeader hdr;
  timeline_item_serialize_header(&item, &hdr);
  return timeline_item_storage_insert(&s_storage, (uint8_t *)&src->header.id, sizeof(Uuid),
                                      (uint8_t *)&hdr, sizeof(hdr), synced) == S_SUCCESS;
}

void test_timeline_item_storage__flush_and_reinit(void) {
  const TimelineItem watch_item = prv_item(1, TIME_PAST_2038, true);
  const TimelineItem phone_item = prv_item(2, TIME_PAST_2038, false);
  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);
  cl_assert(prv_insert(&watch_item, false));
  cl_assert(prv_insert(&phone_item, true));

  cl_assert_equal_i(timeline_item_storage_flush(&s_storage), S_SUCCESS);
  timeline_item_storage_deinit(&s_storage);
  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);

  // Read as stored, not widened a second time
  prv_assert_item(&watch_item);
  cl_assert_equal_i(prv_record(&watch_item).count, 1);

  // Items inserted after the flush are kept as they are too
  cl_assert(prv_insert(&phone_item, true));
  timeline_item_storage_deinit(&s_storage);
  timeline_item_storage_init(&s_storage, FILE_NAME, LEGACY_FILE_NAME, MAX_SIZE, MAX_AGE);
  prv_assert_item(&phone_item);
}
