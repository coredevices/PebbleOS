# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_clar
  MODULES rand
  SOURCES
    pebble/applib/graphics/gtypes.c
)

pbl_clar_test(test_utf8
  SOURCES
    pebble/applib/graphics/utf8.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_utf8_iterator
  SOURCES
    pebble/applib/graphics/utf8.c
)

pbl_clar_test(test_codepoint
  SOURCES
    pebble/applib/fonts/codepoint.c
)

pbl_clar_test(test_bidi
  SOURCES
    pebble/applib/graphics/bidi.c
    pebble/applib/graphics/arabic_shaping.c
    pebble/applib/graphics/utf8.c
    pebble/applib/fonts/codepoint.c
)

pbl_clar_test(test_arabic_shaping
  SOURCES
    pebble/applib/graphics/arabic_shaping.c
    pebble/applib/graphics/utf8.c
)

pbl_clar_test(test_char_iterator
  MODULES text
  SOURCES
    pebble/applib/graphics/framebuffer.c
    pebble/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_word_iterator
  MODULES text
  SOURCES
    pebble/applib/graphics/framebuffer.c
    pebble/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_launcher_app_message
  MODULES rand
  SOURCES
    pebble/process_management/launcher_app_message.c
    pebble/applib/dict.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_line_layout
  MODULES text
  SOURCES
    pebble/applib/graphics/framebuffer.c
    pebble/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
)

pbl_clar_test(test_alarm
  MODULES settings timeline_item time rand
  SOURCES
    pebble/services/alarms/alarm.c
    pebble/services/alarms/alarm_pin.c
    subsys/cron/cron.c
    pebble/services/blob_db/timeline_item_storage.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_alarm_smart
  MODULES settings timeline_item time
  SOURCES
    pebble/services/alarms/alarm.c
    pebble/services/alarms/alarm_pin.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    subsys/cron/cron.c
    pebble/services/blob_db/timeline_item_storage.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_rtc.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_battery_monitor
  SOURCES
    pebble/services/battery/voltage/battery_state.c
    pebble/services/battery/voltage/battery_curve.c
    pebble/services/battery/battery_monitor.c
    tests/fakes/fake_battery.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_SERVICE_BATTERY_LOW_POWER_PERCENT=5 CONFIG_SERVICE_BATTERY_CAPACITY_HOURS=144
  OVERRIDES dummy_board
)

pbl_clar_test(test_alerts
  MODULES settings
  SOURCES
    pebble/services/notifications/alerts_preferences.c
    pebble/services/notifications/alerts.c
    tests/fakes/fake_battery.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_tick)

pbl_clar_test(test_heap
  SOURCES
    pebble/applib/app_heap_util.c
    kernel/heap.c
)

pbl_clar_test(test_data_logging
  MODULES pfs rand
  SOURCES
    pebble/services/data_logging/dls_main.c
    pebble/services/data_logging/dls_list.c
    pebble/services/data_logging/dls_storage.c
    pebble/services/data_logging/dls_endpoint.c
    pebble/services/data_logging/dls_syscalls.c
    tests/fakes/fake_session.c
    tests/fakes/fake_rtc.c
    pebble/applib/data_logging.c
    pebble/services/clock/time.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_mpu_region
  SOURCES
    kernel/mpu_region.c
)

pbl_clar_test(test_app_manager
  MODULES rand
  SOURCES
    pebble/applib/app_exit_reason.c
    pebble/process_management/process_manager.c
    pebble/process_management/pebble_process_info.c
    pebble/process_management/app_manager.c
    pebble/process_management/pebble_process_md.c
    pebble/process_management/process_runtime.c
    pebble/process_management/process_regions.c
    pebble/kernel/util/segment.c
    kernel/heap.c
    kernel/mpu_region.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_process_manager
  SOURCES
    pebble/process_management/pebble_process_info.c
    pebble/process_management/process_manager.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_syscall_define
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
)

pbl_clar_test(test_pebble_app_info
  SOURCES
    pebble/process_management/pebble_process_info.c
)

pbl_clar_test(test_pebble_process_md
  SOURCES
    pebble/process_management/pebble_process_info.c
    pebble/process_management/pebble_process_md.c
  PLATFORMS emery gabbro
)

pbl_clar_test(test_phone_formatting
  SOURCES
    pebble/applib/graphics/utf8.c
    pebble/popups/phone_formatting.c
)

pbl_clar_test(test_timeline_api
  MODULES settings timeline_item time rand
  SOURCES
    pebble/services/blob_db/pin_db.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/timeline.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_blob_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_run_state
  MODULES rand
  SOURCES
    pebble/process_management/app_run_state.c
)

pbl_clar_test(test_text_layout_8_bit
  SOURCE test_text_layout.c
  MODULES graphics text
  SOURCES
    pebble/applib/graphics/8_bit/framebuffer.c
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/8_bit/bitblt_private.c
    tests/fakes/fake_gbitmap_png.c
  PLATFORMS emery
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
)

pbl_clar_test(test_getting_started_button_combo
  SOURCES
    pebble/apps/prf/recovery_first_use/getting_started_button_combo.c
  DEFINES CONFIG_RECOVERY_FW
)

pbl_clar_test(test_event_syscalls
  SOURCES
    pebble/syscall/event_syscalls.c
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
)
