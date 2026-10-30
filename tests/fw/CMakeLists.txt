# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_clar
  MODULES rand
  SOURCES
    fw/applib/graphics/gtypes.c
)

pbl_clar_test(test_utf8
  SOURCES
    fw/applib/graphics/utf8.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_i18n
  MODULES resources
  SOURCES
    fw/services/i18n/i18n.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
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
  MODULES text
  SOURCES
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_word_iterator
  MODULES text
  SOURCES
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_launcher_app_message
  MODULES rand
  SOURCES
    fw/process_management/launcher_app_message.c
    fw/applib/dict.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_line_layout
  MODULES text
  SOURCES
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_alarm
  MODULES settings timeline_item time rand
  SOURCES
    fw/services/alarms/alarm.c
    fw/services/alarms/alarm_pin.c
    subsys/cron/cron.c
    fw/services/blob_db/timeline_item_storage.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_alarm_smart
  MODULES settings timeline_item time
  SOURCES
    fw/services/alarms/alarm.c
    fw/services/alarms/alarm_pin.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    subsys/cron/cron.c
    fw/services/blob_db/timeline_item_storage.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_rtc.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
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
  DEFINES CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT=5 CONFIG_SERVICE_BATTERY_CAPACITY_HOURS=144
  OVERRIDES dummy_board
)

pbl_clar_test(test_alerts
  MODULES settings
  SOURCES
    fw/services/notifications/alerts_preferences.c
    fw/services/notifications/alerts.c
    tests/fakes/fake_battery.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_tick)

pbl_clar_test(test_heap
  SOURCES
    fw/applib/app_heap_util.c
)

pbl_clar_test(test_psram_image_pool
  SOURCES fw/kernel/psram_image_pool.c
  DEFINES CONFIG_PSRAM_RETENTION CONFIG_PSRAM_IMAGE_POOL CONFIG_PSRAM_IMAGE_SLOTS=3
)

pbl_clar_test(test_volatile_heap
  SOURCES fw/kernel/volatile_heap.c
  DEFINES CONFIG_PSRAM_RETENTION CONFIG_KERNEL_VOLATILE_HEAP CONFIG_KERNEL_VOLATILE_HEAP_SIZE=4096
)

pbl_clar_test(test_flash_logging
  SOURCES
    fw/debug/flash_logging.c
    fw/flash_region/flash_region.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_data_logging
  MODULES pfs rand
  SOURCES
    fw/services/data_logging/dls_main.c
    fw/services/data_logging/dls_list.c
    fw/services/data_logging/dls_storage.c
    fw/services/data_logging/dls_endpoint.c
    fw/services/data_logging/dls_syscalls.c
    fw/services/regular_timer/service.c
    tests/fakes/fake_session.c
    tests/fakes/fake_rtc.c
    fw/applib/data_logging.c
    fw/services/clock/time.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_memory_layout
  SOURCES
    fw/kernel/memory_layout.c
)

pbl_clar_test(test_app_manager
  MODULES rand
  SOURCES
    fw/applib/app_exit_reason.c
    fw/process_management/process_manager.c
    fw/process_management/pebble_process_info.c
    fw/process_management/app_manager.c
    fw/process_management/pebble_process_md.c
    fw/process_management/process_runtime.c
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

pbl_clar_test(test_syscall_define
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
)

pbl_clar_test(test_pebble_app_info
  SOURCES
    fw/process_management/pebble_process_info.c
)

pbl_clar_test(test_pebble_process_md
  SOURCES
    fw/process_management/pebble_process_info.c
    fw/process_management/pebble_process_md.c
  PLATFORMS emery gabbro
)

pbl_clar_test(test_phone_formatting
  SOURCES
    fw/applib/graphics/utf8.c
    fw/popups/phone_formatting.c
)

pbl_clar_test(test_timeline_api
  MODULES settings timeline_item time rand
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/timeline.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_blob_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_run_state
  MODULES rand
  SOURCES
    fw/process_management/app_run_state.c
)

pbl_clar_test(test_text_layout_8_bit
  SOURCE test_text_layout.c
  MODULES graphics text
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/8_bit/bitblt_private.c
    tests/fakes/fake_gbitmap_png.c
  PLATFORMS emery
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
)

pbl_clar_test(test_getting_started_button_combo
  SOURCES
    fw/apps/prf/recovery_first_use/getting_started_button_combo.c
  DEFINES CONFIG_RECOVERY_FW
)

pbl_clar_test(test_event_syscalls
  SOURCES
    fw/syscall/event_syscalls.c
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
)
