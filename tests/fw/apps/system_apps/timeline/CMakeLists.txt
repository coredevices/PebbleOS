# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_timeline_list_view
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/system/hexdump.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_graphics_context.c
    tests/fakes/fake_property_animation.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    fw/applib/graphics/gdraw_command_transforms.c
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/scale_segmented.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel/unfold.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/peek_layer.c
    fw/apps/system/timeline/pin_window.c
    fw/apps/system/timeline/text_node.c
    fw/apps/system/timeline/timeline.c
    fw/apps/system/timeline/animations.c
    fw/apps/system/timeline/layer.c
    fw/apps/system/timeline/model.c
    fw/apps/system/timeline/relbar.c
    fw/popups/timeline/timeline_item_layer.c
    fw/services/clock/service.c
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/generic_layout.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_settings_file.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_evented_timer.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_timeline_no_events
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/system/hexdump.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_graphics_context.c
    tests/fakes/fake_property_animation.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    fw/applib/graphics/gdraw_command_transforms.c
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/scale_segmented.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel/unfold.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/peek_layer.c
    fw/apps/system/timeline/pin_window.c
    fw/apps/system/timeline/text_node.c
    fw/apps/system/timeline/timeline.c
    fw/apps/system/timeline/animations.c
    fw/apps/system/timeline/layer.c
    fw/apps/system/timeline/model.c
    fw/apps/system/timeline/relbar.c
    fw/popups/timeline/timeline_item_layer.c
    fw/services/clock/service.c
    fw/services/blob_db/pin_db.c
    fw/services/blob_db/timeline_item_storage.c
    fw/services/timeline/generic_layout.c
    fw/services/timeline/layout_layer.c
    fw/services/timeline/layout_node.c
    fw/services/timeline/timeline.c
    fw/services/timeline/timeline_layout.c
    fw/services/timeline/timeline_resources.c
    fw/shell/system_theme.c
    fw/shell/system_theme_style.c
    tests/fakes/fake_settings_file.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_evented_timer.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
