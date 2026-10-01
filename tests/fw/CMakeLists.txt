# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_clar
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_clar_test(test_utf8
  SOURCES
    fw/applib/graphics/utf8.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_i18n
  SOURCES
    fw/services/i18n/i18n.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    tests/fakes/fake_bootbits.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_utf8_iterator
  SOURCES
    fw/applib/graphics/utf8.c
)

pbl_clar_test(test_codepoint
  SOURCES
    fw/applib/fonts/codepoint.c
)

pbl_clar_test(test_bidi
  SOURCES
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/graphics/utf8.c
    fw/applib/fonts/codepoint.c
)

pbl_clar_test(test_arabic_shaping
  SOURCES
    fw/applib/graphics/arabic_shaping.c
    fw/applib/graphics/utf8.c
)

pbl_clar_test(test_char_iterator
  SOURCES
    fw/applib/graphics/utf8.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_word_iterator
  SOURCES
    fw/applib/graphics/utf8.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_launcher_app_message
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/process_management/launcher_app_message.c
    fw/applib/dict.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_line_layout
  SOURCES
    fw/applib/graphics/utf8.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_alarm
  SOURCES
    fw/services/alarms/alarm.c
    fw/services/alarms/alarm_pin.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    subsys/cron/cron.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_alarm_smart
  SOURCES
    fw/services/alarms/alarm.c
    fw/services/alarms/alarm_pin.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    subsys/cron/cron.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_battery_monitor
  SOURCES
    fw/services/battery/voltage/battery_state.c
    fw/services/battery/voltage/battery_curve.c
    fw/services/battery/battery_monitor.c
    tests/fakes/fake_battery.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_alerts
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/notifications/alerts_preferences.c
    fw/services/notifications/alerts.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_battery.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_tick)

pbl_clar_test(test_heap
  SOURCES
    fw/applib/app_heap_util.c
)

pbl_clar_test(test_flash_logging
  SOURCES
    fw/debug/flash_logging.c
    fw/flash_region/flash_region.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS asterix
  DEFINES DUMA_DISABLED PLATFORM_ASTERIX
  OVERRIDES dummy_board
)

pbl_clar_test(test_data_logging
  SOURCES
    fw/services/data_logging/dls_main.c
    fw/services/data_logging/dls_list.c
    fw/services/data_logging/dls_storage.c
    fw/services/data_logging/dls_endpoint.c
    fw/services/data_logging/dls_syscalls.c
    fw/services/regular_timer/service.c
    tests/fakes/fake_spi_flash.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/flash_translation.c
    tests/fakes/fake_session.c
    tests/fakes/fake_rtc.c
    fw/applib/data_logging.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_memory_layout
  SOURCES
    fw/kernel/memory_layout.c
)

pbl_clar_test(test_app_manager
  SOURCES
    fw/applib/app_exit_reason.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/process_management/process_manager.c
    fw/process_management/pebble_process_info.c
    fw/process_management/app_manager.c
    fw/process_management/pebble_process_md.c
    fw/kernel/memory_layout.c
    fw/kernel/util/segment.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_process_manager
  SOURCES
    fw/process_management/pebble_process_info.c
    fw/process_management/process_manager.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_pebble_app_info
  SOURCES
    fw/process_management/pebble_process_info.c
)

pbl_clar_test(test_pebble_process_md
  SOURCES
    fw/process_management/pebble_process_info.c
    fw/process_management/pebble_process_md.c
  PLATFORMS obelix gabbro
)

pbl_clar_test(test_phone_formatting
  SOURCES
    fw/applib/graphics/utf8.c
    fw/popups/phone_formatting.c
)

pbl_clar_test(test_timeline_api
  SOURCES
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/item.c
    fw/services/timeline/timeline.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/util/rand/rand.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/stubs/stubs_blob_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_run_state
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/process_management/app_run_state.c
)

pbl_clar_test(test_text_layout_8_bit
  SOURCE test_text_layout.c
  SOURCES
    fw/applib/graphics/utf8.c
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/graphics.c
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/graphics_circle.c
    fw/applib/graphics/graphics_line.c
  PLATFORMS obelix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
)

pbl_clar_test(test_getting_started_button_combo
  SOURCES
    fw/apps/prf/recovery_first_use/getting_started_button_combo.c
  DEFINES CONFIG_RECOVERY_FW
)
