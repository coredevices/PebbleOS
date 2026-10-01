# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_alerts_preferences
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/notifications/alerts_preferences.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_notification_storage
  SOURCES
    fw/services/notifications/notification_storage.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attribute.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_ancs_notifications
  SOURCES
    fw/services/notifications/ancs/ancs_notifications.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/notifications/ancs/ancs_item.c
    fw/services/notifications/ancs/ancs_phone_call.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/item.c
    fw/services/phone_call/util.c
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/utf8.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    tests/fakes/fake_events.c
)

pbl_clar_test(test_ancs_notifications_util
  SOURCES
    fw/services/notifications/ancs/ancs_notifications_util.c
    lib/c/time/mktime.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs_filtering
  SOURCES
    fw/services/notifications/ancs/ancs_filtering.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
)

pbl_clar_test(test_nexmo
  SOURCES
    fw/services/notifications/ancs/nexmo.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    tests/fakes/fake_rtc.c
)
