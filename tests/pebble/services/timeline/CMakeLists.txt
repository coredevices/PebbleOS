# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_reminders
  MODULES settings timeline_item time rand
  SOURCES
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/blob_db/reminder_db.c
    pebble/services/timeline/reminders.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_calendar
  MODULES timeline_item time
  SOURCES
    pebble/services/blob_db/pin_db.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/calendar.c
    pebble/services/timeline/event.c
    pebble/services/timeline/timeline.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_peek_event
  MODULES timeline_item time
  SOURCES
    pebble/services/blob_db/pin_db.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/event.c
    pebble/services/timeline/peek.c
    pebble/services/timeline/timeline.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_attribute
  SOURCES
    pebble/services/timeline/attribute.c
    pebble/services/timeline/attribute_group.c
    pebble/services/timeline/attributes_actions.c
)

pbl_clar_test(test_timeline_model
  MODULES settings timeline_item time rand
  SOURCES
    tests/fakes/fake_rtc.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/timeline.c
    pebble/services/blob_db/pin_db.c
    pebble/apps/system/timeline/model.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_resources
  SOURCES
    pebble/process_management/pebble_process_info.c
    pebble/services/timeline/timeline_resources.c
    tests/fixtures/resources/timeline_resource_table.auto.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_alarm_layout
  SOURCES
    pebble/services/alarms/alarm.c
    pebble/services/timeline/alarm_layout.c
    pebble/services/timeline/attribute.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_layouts
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    subsys/logging/hexdump.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    pebble/apps/system/timeline/pin_window.c
    pebble/popups/timeline/timeline_item_layer.c
    pebble/services/timeline/alarm_layout.c
    pebble/services/timeline/attribute.c
    pebble/services/timeline/calendar_layout.c
    pebble/services/timeline/calendar_layout_resources.c
    pebble/services/timeline/generic_layout.c
    pebble/services/timeline/layout_layer.c
    pebble/services/timeline/layout_node.c
    pebble/services/timeline/sports_layout.c
    pebble/services/timeline/timeline_layout.c
    pebble/services/timeline/timeline_resources.c
    pebble/services/timeline/weather_layout.c
    pebble/services/weather/weather_types.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/stubs/stubs_clock.c
    tests/stubs/stubs_timeline_layout.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_swap_layer_touch
  MODULES gtypes
  SOURCES
    pebble/applib/ui/layer.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/touch_nav.c
    pebble/services/timeline/swap_layer.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
  OVERRIDES dummy_board
)
