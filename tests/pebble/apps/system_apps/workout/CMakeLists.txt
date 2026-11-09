# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_workout_summary
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/action_bar_layer.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/shell/system_theme_style.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/dialogs/dialog.c
    pebble/applib/ui/dialogs/dialog_private.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel/scale_segmented.c
    pebble/applib/ui/kino/kino_reel/unfold.c
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/activity/hr_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/apps/system/workout/countdown.c
    pebble/apps/system/workout/dialog.c
    pebble/apps/system/workout/utils.c
    pebble/apps/system/workout/style.c
    pebble/apps/system/workout/summary.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

set(workout_active_args
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/action_bar_layer.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/shell/system_theme_style.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/dialogs/dialog.c
    pebble/applib/ui/dialogs/dialog_private.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/activity/hr_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/apps/system/workout/data.c
    pebble/apps/system/workout/dialog.c
    pebble/apps/system/workout/active.c
    pebble/apps/system/workout/style.c
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  OVERRIDES dummy_board
  TEST_IMAGES
)

# Only emery has a heart rate sensor.
pbl_clar_test(test_workout_active ${workout_active_args}
  PLATFORMS emery
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1 CONFIG_HRM
)

pbl_clar_test(test_workout_active ${workout_active_args}
  PLATFORMS gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
)

pbl_clar_test(test_workout_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/action_bar_layer.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/menu_layer_system_cells.c
    pebble/shell/system_theme_style.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/dialogs/dialog.c
    pebble/applib/ui/dialogs/dialog_private.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/activity/hr_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/apps/system/workout/data.c
    pebble/apps/system/workout/dialog.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_workout_utils
  SOURCES
    pebble/apps/system/workout/utils.c
  OVERRIDES dummy_board
)
