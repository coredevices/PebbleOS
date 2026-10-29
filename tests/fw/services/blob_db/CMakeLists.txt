# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_timeline
  MODULES timeline_item time
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/timeline.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_pin_db
  MODULES timeline_item time
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/blob_db/util.c
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
  MODULES settings rand
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/app_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance_db
  SOURCES
    fw/services/blob_db/app_glance_db.c
    fw/services/timeline/attribute.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_contacts_db
  MODULES settings rand
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/contacts_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_watch_app_prefs_db
  MODULES settings rand
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/watch_app_prefs_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_reminder_db
  MODULES timeline_item time rand
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/timeline_item_storage.c
    tests/fakes/fake_settings_file.c
    fw/services/blob_db/reminder_db.c
    fw/services/blob_db/sync_util.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notif_db
  MODULES pfs timeline_item time rand
  SOURCES
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_rtc.c
    fw/services/notifications/notification_storage.c
    fw/services/blob_db/notif_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_weather_db
  MODULES settings time
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/weather_db.c
    tests/fw/services/blob_db/weather_data_shared.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_prefs_db
  MODULES settings time rand
  SOURCES
    fw/services/blob_db/prefs_db.c
    fw/shell/normal/prefs.c
    tests/stubs/stubs_system_resource.c
    tests/fakes/fake_accel_service.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_SERVICE_ACCEL_MANAGER_DEFAULT_SENSITIVITY=55
  OVERRIDES dummy_board
)

pbl_clar_test(test_ios_notif_pref_db
  MODULES timeline_item time rand
  SOURCES
    lib/util/uuid.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
    fw/services/blob_db/sync_util.c
    fw/services/blob_db/ios_notif_pref_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_health_db
  MODULES time
  SOURCES
    tests/fakes/fake_settings_file.c
    fw/services/blob_db/health_db.c
  OVERRIDES dummy_board
)
