/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/blob_db/ios_notif_pref_db.h"
#include "util/size.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_system_task.h"
#include "fake_settings_file.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_rand_ptr.h"
#include "stubs_pfs.h"
#include "stubs_blob_db_sync.h"
#include "stubs_prompt.h"

extern const char *iOS_NOTIF_PREF_DB_FILE_NAME;
extern const int iOS_NOTIF_PREF_MAX_SIZE;

//! Data from iOS notif pref INSERT
//! 00 00 00 00 00 01 02 0d  03 01 05 00 52 65 70 6c   ........ ....Repl
//! 79 08 71 00 4f 6b 00 59  65 73 00 4e 6f 00 43 61   y.q.Ok.Y es.No.Ca
//! 6c 6c 20 6d 65 00 43 61  6c 6c 20 79 6f 75 20 6c   ll me.Ca ll you l
//! 61 74 65 72 00 54 68 61  6e 6b 20 79 6f 75 00 53   ater.Tha nk you.S
//! 65 65 20 79 6f 75 20 73  6f 6f 6e 00 52 75 6e 6e   ee you s oon.Runn
//! 69 6e 67 20 6c 61 74 65  00 4f 6e 20 6d 79 20 77   ing late .On my w
//! 61 79 00 42 75 73 79 20  72 69 67 68 74 20 6e 6f   ay.Busy  right no
//! 77 20 2d 20 67 69 76 65  20 6d 65 20 61 20 73 65   w - give  me a se
//! 63 6f 6e 64 3f 21 01 00  00                        cond?!.. .

static const uint8_t s_ios_pref_db_insert_dict[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x0d, 0x03, 0x01, 0x05, 0x00, 0x52, 0x65, 0x70, 0x6c,
  0x79, 0x08, 0x71, 0x00, 0x4f, 0x6b, 0x00, 0x59, 0x65, 0x73, 0x00, 0x4e, 0x6f, 0x00, 0x43, 0x61,
  0x6c, 0x6c, 0x20, 0x6d, 0x65, 0x00, 0x43, 0x61, 0x6c, 0x6c, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x6c,
  0x61, 0x74, 0x65, 0x72, 0x00, 0x54, 0x68, 0x61, 0x6e, 0x6b, 0x20, 0x79, 0x6f, 0x75, 0x00, 0x53,
  0x65, 0x65, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x73, 0x6f, 0x6f, 0x6e, 0x00, 0x52, 0x75, 0x6e, 0x6e,
  0x69, 0x6e, 0x67, 0x20, 0x6c, 0x61, 0x74, 0x65, 0x00, 0x4f, 0x6e, 0x20, 0x6d, 0x79, 0x20, 0x77,
  0x61, 0x79, 0x00, 0x42, 0x75, 0x73, 0x79, 0x20, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x6e, 0x6f,
  0x77, 0x20, 0x2d, 0x20, 0x67, 0x69, 0x76, 0x65, 0x20, 0x6d, 0x65, 0x20, 0x61, 0x20, 0x73, 0x65,
  0x63, 0x6f, 0x6e, 0x64, 0x3f, 0x21, 0x01, 0x00, 0x00
};

const uint8_t key[] = { 0x01, 0x02, 0x03 };

void test_ios_notif_pref_db__initialize(void) {
}

void test_ios_notif_pref_db__cleanup(void) {
    fake_settings_file_reset();
}

void test_ios_notif_pref_db__insert_inverts_flags(void) {
  // Make a local copy of dict since ios_notif_pref_db_insert() modifies val
  uint8_t val[sizeof(s_ios_pref_db_insert_dict)];
  memcpy(val, s_ios_pref_db_insert_dict, sizeof(s_ios_pref_db_insert_dict));

  ios_notif_pref_db_insert(key, sizeof(key), val, sizeof(val));

  SettingsFile file;
  settings_file_open(&file, iOS_NOTIF_PREF_DB_FILE_NAME, iOS_NOTIF_PREF_MAX_SIZE);

  const unsigned prefs_len = settings_file_get_len(&file, key, sizeof(key));
  void *prefs_out = kernel_zalloc(prefs_len);
  settings_file_get(&file, key, sizeof(key), prefs_out, prefs_len);
  settings_file_close(&file);

  uint32_t flags = *((uint32_t *) prefs_out);
  cl_assert_equal_i(flags, ~0);

  kernel_free(prefs_out);
}

void test_ios_notif_pref_db__read_flags(void) {
  // Make a local copy of dict since ios_notif_pref_db_insert() modifies val
  uint8_t val[sizeof(s_ios_pref_db_insert_dict)];
  memcpy(val, s_ios_pref_db_insert_dict, sizeof(s_ios_pref_db_insert_dict));

  ios_notif_pref_db_insert(key, sizeof(key), val, sizeof(val));

  uint32_t flags = ios_notif_pref_db_get_flags(key, sizeof(key));
  cl_assert_equal_i(flags, 0);
}

void test_ios_notif_pref_db__store_prefs(void) {
  // Create an attribute list and action group
  AttributeList attr_list;
  attribute_list_init_list(0, &attr_list);
  attribute_list_add_cstring(&attr_list, AttributeIdShortTitle, "Title");
  attribute_list_add_uint8(&attr_list, AttributeIdMuteDayOfWeek, 0x1F);
  attribute_list_add_cstring(&attr_list, AttributeIdAppName, "GMail");
  TimelineItemActionGroup action_group = {
    .num_actions = 0,
  };

  // Store them in the DB
  char *key = "key1";
  int key_len = strlen(key);
  ios_notif_pref_db_store_prefs((uint8_t *)key, key_len, &attr_list, &action_group);

  // Make sure we can get the data back
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs((uint8_t *)key, key_len);
  cl_assert(notif_prefs);
  Attribute *title = attribute_find(&notif_prefs->attr_list, AttributeIdShortTitle);
  cl_assert(title);
  cl_assert_equal_s(title->cstring, "Title");
  Attribute *mute = attribute_find(&notif_prefs->attr_list, AttributeIdMuteDayOfWeek);
  cl_assert(mute);
  cl_assert_equal_i(mute->uint8, 0x1F);
  Attribute *name = attribute_find(&notif_prefs->attr_list, AttributeIdAppName);
  cl_assert(name);
  cl_assert_equal_s(name->cstring, "GMail");


  // Update the current entry with a new attribute
  attribute_list_add_uint32(&attr_list, AttributeIdLastUpdated, 123456);
  ios_notif_pref_db_store_prefs((uint8_t *)key, key_len, &attr_list, &action_group);

  // Make sure we can get all the data back
  notif_prefs = ios_notif_pref_db_get_prefs((uint8_t *)key, key_len);
  cl_assert(notif_prefs);
  title = attribute_find(&notif_prefs->attr_list, AttributeIdShortTitle);
  cl_assert(title);
  cl_assert_equal_s(title->cstring, "Title");
  mute = attribute_find(&notif_prefs->attr_list, AttributeIdMuteDayOfWeek);
  cl_assert(mute);
  cl_assert_equal_i(mute->uint8, 0x1F);
  name = attribute_find(&notif_prefs->attr_list, AttributeIdAppName);
  cl_assert(name);
  cl_assert_equal_s(name->cstring, "GMail");
  Attribute *updated = attribute_find(&notif_prefs->attr_list, AttributeIdLastUpdated);
  cl_assert(updated);
  cl_assert_equal_i(updated->uint32, 123456);

  attribute_list_destroy_list(&attr_list);
  ios_notif_pref_db_free_prefs(notif_prefs);
}

void test_ios_notif_pref_db__store_empty_prefs(void) {
  // Store empty prefs
  char *key = "key1";
  int key_len = strlen(key);
  ios_notif_pref_db_store_prefs((uint8_t *)key, key_len, NULL, NULL);

  // Read them back
  iOSNotifPrefs *notif_prefs = ios_notif_pref_db_get_prefs((uint8_t *)key, key_len);
  cl_assert(notif_prefs);
  cl_assert_equal_i(notif_prefs->attr_list.num_attributes, 0);
  cl_assert_equal_i(notif_prefs->action_group.num_actions, 0);

  ios_notif_pref_db_free_prefs(notif_prefs);
}

void test_ios_notif_pref_db__is_dirty_insert_from_phone(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);
  char *values[] = { "val1", "val2", "val3", "val4", "val5" };
  int value_len = strlen(values[0]);

  // Insert a bunch of known apps "from the phone"
  // They should NOT be dirty (the phone is the source of truth)
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    ios_notif_pref_db_insert((uint8_t *)keys[i], key_len, (uint8_t *)values[i], value_len);
  }

  bool is_dirty = true;
  cl_assert_equal_i(ios_notif_pref_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(!is_dirty);

  BlobDBDirtyItem *dirty_list = ios_notif_pref_db_get_dirty_list();
  cl_assert(!dirty_list);
}

void test_ios_notif_pref_db__is_dirty_insert_locally(void) {
  char *keys[] = { "key1", "key2", "key3", "key4", "key5" };
  int key_len = strlen(keys[0]);

  // Insert a bunch of known apps "from the watch"
  // These should be dirty (the phone is the source of truth)
  for (int i = 0; i < ARRAY_LENGTH(keys); ++i) {
    ios_notif_pref_db_store_prefs((uint8_t *)keys[i], key_len, NULL, NULL);
  }

  bool is_dirty = false;
  cl_assert_equal_i(ios_notif_pref_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(is_dirty);

  BlobDBDirtyItem *dirty_list = ios_notif_pref_db_get_dirty_list();
  cl_assert(dirty_list);
  cl_assert(list_count((ListNode *)dirty_list) == ARRAY_LENGTH(keys));

  // Mark some items as synced
  ios_notif_pref_db_mark_synced((uint8_t *)keys[0], key_len);
  ios_notif_pref_db_mark_synced((uint8_t *)keys[1], key_len);
  ios_notif_pref_db_mark_synced((uint8_t *)keys[2], key_len);

  // We should now only have 2 dirty items
  cl_assert_equal_i(ios_notif_pref_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(is_dirty);

  dirty_list = ios_notif_pref_db_get_dirty_list();
  cl_assert(dirty_list);
  cl_assert_equal_i(list_count((ListNode *)dirty_list), 2);

  // Mark the final 2 items as synced
  ios_notif_pref_db_mark_synced((uint8_t *)keys[3], key_len);
  ios_notif_pref_db_mark_synced((uint8_t *)keys[4], key_len);

  // And nothing should be dirty
  cl_assert_equal_i(ios_notif_pref_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(!is_dirty);

  dirty_list = ios_notif_pref_db_get_dirty_list();
  cl_assert(!dirty_list);
}
