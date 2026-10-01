# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_reminders
  SOURCES
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/flash_region/flash_region.c
    fw/util/time/time.c
    fw/flash_region/filesystem_regions.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/reminder_db.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/reminders.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_calendar
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/calendar.c
    fw/services/timeline/event.c
    fw/services/timeline/item.c
    fw/services/timeline/timeline.c
    fw/util/crc8.c
    fw/util/time/time.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_peek_event
  SOURCES
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/attribute.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/event.c
    fw/services/timeline/item.c
    fw/services/timeline/peek.c
    fw/services/timeline/timeline.c
    fw/util/crc8.c
    fw/util/time/time.c
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
  SOURCES
    fw/util/time/time.c
    fw/util/time/mktime.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    tests/fakes/fake_spi_flash.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/timeline/attribute.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
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
  PLATFORMS obelix gabbro
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_timeline_layouts
  SOURCES
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gbitmap_png.c
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_sequence.c
    fw/applib/graphics/gpath.c
    fw/applib/graphics/graphics.c
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/graphics/graphics_circle.c
    fw/applib/graphics/graphics_line.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/perimeter.c
    fw/applib/graphics/text_layout.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/graphics/text_render.c
    fw/applib/graphics/text_resources.c
    fw/applib/graphics/utf8.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/inverter_layer.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/kino/kino_reel_gbitmap.c
    fw/applib/ui/kino/kino_reel_gbitmap_sequence.c
    fw/applib/ui/kino/kino_reel_pdci.c
    fw/applib/ui/kino/kino_reel_pdcs.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/text_layer.c
    fw/applib/ui/text_layer_flow.c
    fw/applib/ui/window.c
    fw/applib/vendor/tinflate/tinflate.c
    fw/applib/vendor/uPNG/upng.c
    fw/apps/system/timeline/text_node.c
    fw/board/displays/display_getafix.c
    fw/drivers/flash/flash_crc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/system/hexdump.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/util/time/time.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_display.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_animation.c
    fw/apps/system/timeline/pin_window.c
    fw/popups/timeline/timeline_item_layer.c
    fw/services/timeline/attribute.c
    fw/services/timeline/generic_layout.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/sports_layout.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/services/timeline/weather_layout.c
    fw/services/weather/weather_types.c
    fw/shell/system_theme.c
    tests/stubs/stubs_clock.c
    tests/stubs/stubs_timeline_layout.c
  PLATFORMS obelix gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_swap_layer_touch
  SOURCES
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gtypes.c
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
