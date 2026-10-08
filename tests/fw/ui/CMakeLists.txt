# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_layer
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/graphics_private_raw.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/ui/layer.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_layer_rect
  SOURCES
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/ui/layer.c
)

pbl_clar_test(test_scroll_layer
  SOURCES
    fw/applib/graphics/gtypes.c
    tests/stubs/stubs_animation.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/layer.c
    fw/applib/ui/shadows.c
    fw/applib/ui/scroll_layer.c
)

pbl_clar_test(test_scroll_layer_touch
  MODULES gtypes
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    fw/applib/ui/layer.c
    fw/applib/ui/shadows.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_status_bar_layer
  MODULES graphics text
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/graphics/text_render.c
    fw/applib/graphics/text_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/ui/layer.c
    fw/applib/ui/status_bar_layer.c
    fw/services/clock/time.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    tests/fakes/fake_rtc.c
  DEFINES FONT_KEY_GOTHIC_14="RESOURCE_ID_GOTHIC_14"
)

pbl_clar_test(test_animation
  MODULES gtypes
  SOURCES
    fw/applib/ui/animation.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    fw/applib/ui/property_animation.c
    fw/applib/legacy2/ui/animation_legacy2.c
    fw/applib/legacy2/ui/property_animation_legacy2.c
    fw/applib/ui/layer.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_gbitmap_png.c
    fw/services/animation_service/service.c
  DEFINES DUMA_DISABLED
)

pbl_clar_test(test_animation_interpolate
  SOURCES
    fw/applib/ui/animation_interpolate.c
)

pbl_clar_test(test_animation_timing
  SOURCES
    fw/applib/ui/animation_timing.c
)

pbl_clar_test(test_kino_reel
  MODULES png gtypes
  SOURCES
    fw/applib/ui/layer.c
    fw/applib/ui/kino/kino_reel.c
    fw/applib/ui/kino/kino_reel_pdci.c
    fw/applib/ui/kino/kino_reel_pdcs.c
    fw/applib/ui/kino/kino_reel_gbitmap.c
    fw/applib/ui/kino/kino_reel_gbitmap_sequence.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_sequence.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_rtc.c
  TEST_IMAGES
)

pbl_clar_test(test_kino_player
  MODULES png gtypes
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/ui/layer.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel.c
    fw/applib/ui/kino/kino_reel_pdci.c
    fw/applib/ui/kino/kino_reel_pdcs.c
    fw/applib/ui/kino/kino_reel_gbitmap.c
    fw/applib/ui/kino/kino_reel_gbitmap_sequence.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_sequence.c
    tests/stubs/stubs_animation.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_rtc.c
  TEST_IMAGES
)

pbl_clar_test(test_window_stack
  MODULES gtypes
  SOURCES
    fw/applib/ui/app_window_stack.c
    fw/applib/ui/layer.c
    fw/applib/ui/window.c
    fw/applib/ui/window_manager.c
    fw/applib/ui/window_stack.c
    fw/kernel/ui/modals/modal_manager.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_modal_touch_nav
  MODULES gtypes
  SOURCES
    fw/applib/ui/app_window_stack.c
    fw/applib/ui/layer.c
    fw/applib/ui/window.c
    fw/applib/ui/window_manager.c
    fw/applib/ui/window_stack.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/touch_nav.c
    fw/kernel/ui/modals/modal_manager.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_TOUCH
  OVERRIDES dummy_board
)

pbl_clar_test(test_content_indicator
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/ui/layer.c
    fw/applib/ui/content_indicator.c
)

pbl_clar_test(test_menu_layer
  MODULES gtypes
  SOURCES
    fw/applib/ui/layer.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/menu_layer.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
)

pbl_clar_test(test_action_menu_layer
  MODULES gtypes
  SOURCES
    fw/applib/ui/action_menu_layer.c
    fw/applib/ui/layer.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/menu_layer.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    fw/applib/ui/recognizer/recognizer.c
    fw/applib/ui/recognizer/recognizer_manager.c
    fw/applib/ui/recognizer/pan.c
    fw/applib/ui/recognizer/swipe.c
    fw/applib/ui/recognizer/tap.c
    fw/applib/ui/recognizer/touch_nav.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
  DEFINES CONFIG_TOUCH
  PLATFORMS obelix gabbro
)

pbl_clar_test(test_menu_text_scroll
  SOURCES
    fw/applib/ui/menu_text_scroll.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_menu_layer_system_cells
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fixtures/resources/timeline_resource_table.auto.c
  PLATFORMS obelix gabbro asterix
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_action_menu_window
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/action_menu_hierarchy.c
    fw/applib/ui/action_menu_layer.c
    fw/applib/ui/action_menu_window.c
    fw/applib/ui/crumbs_layer.c
    fw/applib/ui/window.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/content_indicator.c
    tests/fakes/fake_firmware_emoji_resources.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    fw/applib/ui/shadows.c
  DEFINES "FIRMWARE_EMOJI_DIR=\"${PBL_BASE}/resources/normal/base/pbf\""
  PLATFORMS obelix gabbro asterix
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_notification_window
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/action_button.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/popups/notifications/notification_window.c
    fw/popups/notifications/notifications_presented_list.c
    fw/services/notifications/notification_image.c
    fw/services/timeline/attribute.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/notification_layout.c
    fw/services/timeline/swap_layer.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_graphics_context.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_clock.c
    fw/services/clock/time.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_simple_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/shell/system_theme_style.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/dialogs/simple_dialog.c
    fw/applib/ui/dialogs/dialog.c
    fw/applib/ui/dialogs/dialog_private.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/window.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/content_indicator.c
  PLATFORMS obelix gabbro asterix
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_emoji_fonts
  MODULES resources graphics_bitdepth gdraw png text
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/perimeter.c
    fw/applib/graphics/text_render.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/shell/system_theme_style.c
    fw/board/displays/display_getafix.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/fonts/fonts.c
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_expandable_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/shell/system_theme_style.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/action_bar_layer.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/dialogs/dialog.c
    fw/applib/ui/dialogs/dialog_private.c
    fw/applib/ui/dialogs/expandable_dialog.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
  PLATFORMS gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_text_layer
  MODULES gtypes
  SOURCES
    fw/applib/ui/layer.c
    fw/applib/ui/text_layer.c
)

pbl_clar_test(test_selection_windows
  MODULES resources graphics_bitdepth gdraw text_render png
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/date_time_selection_window_private.c
    fw/applib/ui/selection_layer.c
    fw/applib/ui/text_layer.c
    fw/applib/ui/time_range_selection_window.c
    fw/applib/ui/time_selection_window.c
    fw/applib/ui/window.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    fw/services/clock/time.c
  PLATFORMS obelix gabbro asterix
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_text_layer_flow
  MODULES gtypes text_layer
  SOURCES
    fw/applib/ui/layer.c
)

pbl_clar_test(test_jumboji
  SOURCES
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/utf8.c
    fw/services/timeline/notification_layout.c
)

pbl_clar_test(test_timeline_peek
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/applib/ui/menu_text_scroll.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/popups/timeline/peek.c
    fw/services/timeline/generic_layout.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_clock.c
    tests/stubs/stubs_timeline_layout.c
    tests/stubs/stubs_timeline_peek.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_option_menu_window
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/option_menu_window.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/window.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_graphics_context.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    fw/services/clock/time.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
