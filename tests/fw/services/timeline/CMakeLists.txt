# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_reminders
  MODULES settings timeline_item time rand
  SOURCES
    fw/services/blob_db/timeline_item_storage.c
    fw/services/blob_db/reminder_db.c
    fw/services/timeline/reminders.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_calendar
  MODULES timeline_item time
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/calendar.c
    fw/services/timeline/event.c
    fw/services/timeline/timeline.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_peek_event
  MODULES timeline_item time
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/event.c
    fw/services/timeline/peek.c
    fw/services/timeline/timeline.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_attribute
  SOURCES
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
)

pbl_clar_test(test_timeline_model
  MODULES settings timeline_item time rand
  SOURCES
    tests/fakes/fake_rtc.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/timeline.c
    fw/services/blob_db/pin_db.c
    fw/apps/system/timeline/model.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_resources
  SOURCES
    fw/process_management/pebble_process_info.c
    fw/services/timeline/timeline_resources.c
    tests/fixtures/resources/timeline_resource_table.auto.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_alarm_layout
  SOURCES
    fw/services/alarms/alarm.c
    fw/services/timeline/alarm_layout.c
    fw/services/timeline/attribute.c
    fw/services/clock/time.c
  PLATFORMS emery gabbro
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_layouts
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/system/hexdump.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    fw/apps/system/timeline/pin_window.c
    fw/popups/timeline/timeline_item_layer.c
    fw/services/timeline/alarm_layout.c
    fw/services/timeline/attribute.c
    fw/services/timeline/calendar_layout.c
    fw/services/timeline/calendar_layout_resources.c
    fw/services/timeline/generic_layout.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/sports_layout.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/services/timeline/weather_layout.c
    fw/services/weather/weather_types.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/stubs/stubs_clock.c
    tests/stubs/stubs_timeline_layout.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_swap_layer_touch
  MODULES gtypes
  SOURCES
    fw/applib/ui/layer.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/touch_nav.c
    fw/services/timeline/swap_layer.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
  OVERRIDES dummy_board
)
