# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_timeline_list_view
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    subsys/logging/hexdump.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_graphics_context.c
    tests/fakes/fake_property_animation.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/scale_segmented.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel/unfold.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/peek_layer.c
    pebble/apps/system/timeline/pin_window.c
    pebble/apps/system/timeline/text_node.c
    pebble/apps/system/timeline/timeline.c
    pebble/apps/system/timeline/animations.c
    pebble/apps/system/timeline/layer.c
    pebble/apps/system/timeline/model.c
    pebble/apps/system/timeline/relbar.c
    pebble/popups/timeline/timeline_item_layer.c
    pebble/services/clock/service.c
    pebble/services/blob_db/pin_db.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/generic_layout.c
    pebble/services/timeline/layout_layer.c
    pebble/services/timeline/layout_node.c
    pebble/services/timeline/timeline.c
    pebble/services/timeline/timeline_layout.c
    pebble/services/timeline/timeline_resources.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_settings.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/fixtures/screen_grid.c
    tests/stubs/stubs_evented_timer.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_timeline_no_events
  MODULES resources graphics_bitdepth gdraw text_render png timeline_item menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    subsys/logging/hexdump.c
    tests/fakes/fake_animation.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_graphics_context.c
    tests/fakes/fake_property_animation.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/scale_segmented.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel/unfold.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/peek_layer.c
    pebble/apps/system/timeline/pin_window.c
    pebble/apps/system/timeline/text_node.c
    pebble/apps/system/timeline/timeline.c
    pebble/apps/system/timeline/animations.c
    pebble/apps/system/timeline/layer.c
    pebble/apps/system/timeline/model.c
    pebble/apps/system/timeline/relbar.c
    pebble/popups/timeline/timeline_item_layer.c
    pebble/services/clock/service.c
    pebble/services/blob_db/pin_db.c
    pebble/services/blob_db/timeline_item_storage.c
    pebble/services/timeline/generic_layout.c
    pebble/services/timeline/layout_layer.c
    pebble/services/timeline/layout_node.c
    pebble/services/timeline/timeline.c
    pebble/services/timeline/timeline_layout.c
    pebble/services/timeline/timeline_resources.c
    pebble/shell/system_theme.c
    pebble/shell/system_theme_style.c
    tests/fakes/fake_settings.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    tests/stubs/stubs_evented_timer.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
