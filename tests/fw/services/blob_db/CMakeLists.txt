# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_timeline
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/item.c
    fw/services/timeline/timeline.c
    fw/util/crc8.c
    fw/util/time/mktime.c
    fw/util/time/time.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_pin_db
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/blob_db/util.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/item.c
    fw/util/crc8.c
    fw/util/time/mktime.c
    fw/util/time/time.c
    tests/fakes/fake_blobdb.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_session.c
    tests/fakes/fake_settings_file.c
    tests/fakes/ram_storage.c
    tests/fakes/test_db.c
)

pbl_clar_test(test_blob_db_sync
  SOURCES
    fw/services/blob_db/util.c
    fw/services/blob_db/sync.c
    tests/fakes/ram_storage.c
    tests/fakes/test_db.c
    tests/fakes/fake_blobdb.c
)

pbl_clar_test(test_blob_db_endpoint
  SOURCES
    fw/services/blob_db/api.c
    fw/services/blob_db/endpoint.c
    fw/services/blob_db/endpoint_private.c
    fw/services/blob_db/util.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_blob_db2_endpoint
  SOURCES
    fw/services/blob_db/api.c
    fw/services/blob_db/endpoint2.c
    fw/services/blob_db/endpoint_private.c
    fw/services/blob_db/util.c
)

pbl_clar_test(test_app_db
  SOURCES
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/app_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance_db
  SOURCES
    fw/services/blob_db/app_glance_db.c
    fw/services/timeline/attribute.c
    fw/util/crc8.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_contacts_db
  SOURCES
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/contacts_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_watch_app_prefs_db
  SOURCES
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/watch_app_prefs_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_reminder_db
  SOURCES
    fw/util/crc8.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/util/time/time.c
    fw/services/blob_db/timeline_item_storage.c
    tests/fakes/fake_settings_file.c
    fw/services/blob_db/reminder_db.c
    fw/services/blob_db/sync_util.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/item.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notif_db
  SOURCES
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_kernel_services_notifications.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/util/time/time.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/notifications/notification_storage.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/blob_db/notif_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_weather_db
  SOURCES
    fw/util/time/time.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/weather_db.c
    tests/fw/services/blob_db/weather_data_shared.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_prefs_db
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/util/time/time.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/blob_db/prefs_db.c
    fw/shell/normal/prefs.c
    tests/stubs/stubs_i18n.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_ios_notif_pref_db
  SOURCES
    fw/util/crc8.c
    lib/util/uuid.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
    fw/util/time/time.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/ios_notif_pref_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_health_db
  SOURCES
    fw/util/crc8.c
    fw/util/time/time.c
    tests/fakes/fake_settings_file.c
    fw/services/blob_db/health_db.c
  OVERRIDES dummy_board
)
