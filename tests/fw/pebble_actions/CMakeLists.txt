# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ancs_pebble_actions
  SOURCES
    fw/comm/ble/kernel_le_client/ancs/ancs_util.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/services/blob_db/ios_notif_pref_db.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/notifications/ancs/ancs_filtering.c
    fw/services/notifications/ancs/ancs_item.c
    fw/services/notifications/ancs/ancs_notifications.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/notifications/ancs/ancs_phone_call.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/timeline/actions_endpoint.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/item.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_actions.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/util/rand/rand.c
    fw/util/time/time.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_actions
  SOURCES
    fw/services/timeline/actions_endpoint.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/item.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_actions.c
    fw/util/time/time.c
  OVERRIDES dummy_board
)
