# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_workout_summary
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/action_bar_layer.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/shell/system_theme_style.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/dialogs/dialog.c
    fw/applib/ui/dialogs/dialog_private.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel/scale_segmented.c
    fw/applib/ui/kino/kino_reel/unfold.c
    fw/applib/graphics/gdraw_command_transforms.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/activity/hr_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/applib/ui/menu_text_scroll.c
    fw/apps/system/workout/countdown.c
    fw/apps/system/workout/dialog.c
    fw/apps/system/workout/utils.c
    fw/apps/system/workout/style.c
    fw/apps/system/workout/summary.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

set(workout_active_args
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/action_bar_layer.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/shell/system_theme_style.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/dialogs/dialog.c
    fw/applib/ui/dialogs/dialog_private.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/activity/hr_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/applib/ui/menu_text_scroll.c
    fw/apps/system/workout/data.c
    fw/apps/system/workout/dialog.c
    fw/apps/system/workout/active.c
    fw/apps/system/workout/style.c
    fw/applib/graphics/gdraw_command_transforms.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  OVERRIDES dummy_board
  TEST_IMAGES
)

# Only emery has a heart rate sensor.
pbl_clar_test(test_workout_active ${workout_active_args}
  PLATFORMS emery
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1 CONFIG_SERVICE_HRM
)

pbl_clar_test(test_workout_active ${workout_active_args}
  PLATFORMS gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
)

pbl_clar_test(test_workout_dialog
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/action_bar_layer.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/layer.c
    fw/applib/ui/menu_layer_system_cells.c
    fw/shell/system_theme_style.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/dialogs/dialog.c
    fw/applib/ui/dialogs/dialog_private.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/activity/hr_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_workout_service.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/applib/ui/menu_text_scroll.c
    fw/apps/system/workout/data.c
    fw/apps/system/workout/dialog.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_workout_utils
  SOURCES
    fw/apps/system/workout/utils.c
  OVERRIDES dummy_board
)
