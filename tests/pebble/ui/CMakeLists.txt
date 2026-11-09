# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_layer
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/graphics/graphics_private_raw.c
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/ui/layer.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_layer_rect
  SOURCES
    pebble/applib/graphics/graphics_private_raw.c
    pebble/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/ui/layer.c
)

pbl_clar_test(test_scroll_layer
  SOURCES
    pebble/applib/graphics/gtypes.c
    tests/stubs/stubs_animation.c
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/scroll_layer.c
)

pbl_clar_test(test_scroll_layer_touch
  MODULES gtypes
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/animation_timing.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_status_bar_layer
  MODULES graphics text
  SOURCES
    pebble/applib/graphics/8_bit/framebuffer.c
    pebble/applib/graphics/8_bit/bitblt_private.c
    pebble/applib/graphics/text_render.c
    pebble/applib/graphics/text_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/status_bar_layer.c
    pebble/services/clock/time.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    tests/fakes/fake_rtc.c
  DEFINES FONT_KEY_GOTHIC_14="RESOURCE_ID_GOTHIC_14"
)

pbl_clar_test(test_animation
  MODULES gtypes
  SOURCES
    pebble/applib/ui/animation.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/animation_timing.c
    pebble/applib/ui/property_animation.c
    pebble/applib/legacy2/ui/animation_legacy2.c
    pebble/applib/legacy2/ui/property_animation_legacy2.c
    pebble/applib/ui/layer.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_gbitmap_png.c
    pebble/services/animation_service/service.c
  DEFINES DUMA_DISABLED
)

pbl_clar_test(test_animation_interpolate
  SOURCES
    pebble/applib/ui/animation_interpolate.c
)

pbl_clar_test(test_animation_timing
  SOURCES
    pebble/applib/ui/animation_timing.c
)

pbl_clar_test(test_kino_reel
  MODULES png gtypes
  SOURCES
    pebble/applib/ui/layer.c
    pebble/applib/ui/kino/kino_reel.c
    pebble/applib/ui/kino/kino_reel_pdci.c
    pebble/applib/ui/kino/kino_reel_pdcs.c
    pebble/applib/ui/kino/kino_reel_gbitmap.c
    pebble/applib/ui/kino/kino_reel_gbitmap_sequence.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/graphics/gbitmap.c
    pebble/applib/graphics/gbitmap_sequence.c
    pebble/applib/graphics/gdraw_command.c
    pebble/applib/graphics/gdraw_command_list.c
    pebble/applib/graphics/gdraw_command_image.c
    pebble/applib/graphics/gdraw_command_frame.c
    pebble/applib/graphics/gdraw_command_sequence.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_rtc.c
  TEST_IMAGES
)

pbl_clar_test(test_kino_player
  MODULES png gtypes
  SOURCES
    pebble/applib/graphics/gbitmap.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel.c
    pebble/applib/ui/kino/kino_reel_pdci.c
    pebble/applib/ui/kino/kino_reel_pdcs.c
    pebble/applib/ui/kino/kino_reel_gbitmap.c
    pebble/applib/ui/kino/kino_reel_gbitmap_sequence.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/graphics/gbitmap_sequence.c
    pebble/applib/graphics/gdraw_command.c
    pebble/applib/graphics/gdraw_command_list.c
    pebble/applib/graphics/gdraw_command_image.c
    pebble/applib/graphics/gdraw_command_frame.c
    pebble/applib/graphics/gdraw_command_sequence.c
    tests/stubs/stubs_animation.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_rtc.c
  TEST_IMAGES
)

pbl_clar_test(test_window_stack
  MODULES gtypes
  SOURCES
    pebble/applib/ui/app_window_stack.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/window.c
    pebble/applib/ui/window_manager.c
    pebble/applib/ui/window_stack.c
    pebble/kernel/ui/modals/modal_manager.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_modal_touch_nav
  MODULES gtypes
  SOURCES
    pebble/applib/ui/app_window_stack.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/window.c
    pebble/applib/ui/window_manager.c
    pebble/applib/ui/window_stack.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/touch_nav.c
    pebble/kernel/ui/modals/modal_manager.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
  OVERRIDES dummy_board
)

pbl_clar_test(test_content_indicator
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/content_indicator.c
)

pbl_clar_test(test_menu_layer
  MODULES gtypes
  SOURCES
    pebble/applib/ui/layer.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/menu_layer.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/animation_timing.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_action_menu_layer
  MODULES gtypes
  SOURCES
    pebble/applib/ui/action_menu_layer.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/menu_layer.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/animation_timing.c
    pebble/applib/ui/recognizer/recognizer.c
    pebble/applib/ui/recognizer/recognizer_manager.c
    pebble/applib/ui/recognizer/pan.c
    pebble/applib/ui/recognizer/swipe.c
    pebble/applib/ui/recognizer/tap.c
    pebble/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
  PLATFORMS emery gabbro
)

pbl_clar_test(test_menu_text_scroll
  SOURCES
    pebble/applib/ui/menu_text_scroll.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_menu_layer_system_cells
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_fonts.c
    tests/fixtures/screen_grid.c
    tests/fixtures/resources/timeline_resource_table.auto.c
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_action_menu_window
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/action_menu_hierarchy.c
    pebble/applib/ui/action_menu_layer.c
    pebble/applib/ui/action_menu_window.c
    pebble/applib/ui/crumbs_layer.c
    pebble/applib/ui/window.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/content_indicator.c
    tests/fakes/fake_firmware_emoji_resources.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    pebble/applib/ui/shadows.c
  DEFINES "FIRMWARE_EMOJI_DIR=\"${PBL_BASE}/resources/normal/base/pbf\""
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_notification_window
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/preferred_durations.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    pebble/popups/notifications/notification_window.c
    pebble/popups/notifications/notifications_presented_list.c
    pebble/services/notifications/notification_image.c
    pebble/services/timeline/attribute.c
    pebble/services/timeline/layout_layer.c
    pebble/services/timeline/layout_node.c
    pebble/services/timeline/notification_layout.c
    pebble/services/timeline/swap_layer.c
    pebble/services/timeline/timeline_layout.c
    pebble/services/timeline/timeline_resources.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_graphics_context.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_clock.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_simple_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/shell/system_theme_style.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/dialogs/simple_dialog.c
    pebble/applib/ui/dialogs/dialog.c
    pebble/applib/ui/dialogs/dialog_private.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/window.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/content_indicator.c
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_emoji_fonts
  MODULES resources graphics_bitdepth gdraw png text
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/perimeter.c
    pebble/applib/graphics/text_render.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_round_display.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    tests/fakes/fake_fonts.c
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_emoji_glyphs
  MODULES resources graphics_bitdepth gdraw png text
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/perimeter.c
    pebble/applib/graphics/text_render.c
    pebble/applib/graphics/text_resources.c
    tests/fakes/fake_round_display.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
  DEFINES "FIRMWARE_EMOJI_DIR=\"${PBL_BASE}/resources/normal/base/pbf\""
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_expandable_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/shell/system_theme_style.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/action_bar_layer.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/dialogs/dialog.c
    pebble/applib/ui/dialogs/dialog_private.c
    pebble/applib/ui/dialogs/expandable_dialog.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
  PLATFORMS gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_text_layer
  MODULES gtypes
  SOURCES
    pebble/applib/ui/layer.c
    pebble/applib/ui/text_layer.c
)

pbl_clar_test(test_selection_windows
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/date_time_selection_window_private.c
    pebble/applib/ui/selection_layer.c
    pebble/applib/ui/text_layer.c
    pebble/applib/ui/time_range_selection_window.c
    pebble/applib/ui/time_selection_window.c
    pebble/applib/ui/window.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_text_layer_flow
  MODULES gtypes text_layer
  SOURCES
    pebble/applib/ui/layer.c
)

pbl_clar_test(test_jumboji
  SOURCES
    pebble/applib/fonts/codepoint.c
    pebble/applib/graphics/utf8.c
    pebble/services/timeline/notification_layout.c
)

pbl_clar_test(test_timeline_peek
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/applib/ui/menu_text_scroll.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    pebble/popups/timeline/peek.c
    pebble/services/timeline/generic_layout.c
    pebble/services/timeline/layout_layer.c
    pebble/services/timeline/layout_node.c
    pebble/services/timeline/timeline_layout.c
    pebble/services/timeline/timeline_resources.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_clock.c
    tests/stubs/stubs_timeline_layout.c
    tests/stubs/stubs_timeline_peek.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_option_menu_window
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/option_menu_window.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/window.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_graphics_context.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
