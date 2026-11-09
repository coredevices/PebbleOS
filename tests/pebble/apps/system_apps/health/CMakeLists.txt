# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_health_card_view
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/activity_detail_card.c
    pebble/apps/system/health/activity_summary_card.c
    pebble/apps/system/health/hr_detail_card.c
    pebble/apps/system/health/hr_chart.c
    pebble/apps/system/health/hr_summary_card.c
    pebble/apps/system/health/sleep_detail_card.c
    pebble/apps/system/health/sleep_summary_card.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/card_view.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_activity_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/activity_detail_card.c
    pebble/apps/system/health/activity_summary_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_sleep_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/sleep_detail_card.c
    pebble/apps/system/health/sleep_summary_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/hr_detail_card.c
    pebble/apps/system/health/hr_chart.c
    pebble/apps/system/health/hr_summary_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_activity_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/activity_detail_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_sleep_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/sleep_detail_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/apps/system/health/hr_detail_card.c
    pebble/apps/system/health/hr_chart.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    pebble/applib/graphics/gpath_builder.c
    pebble/applib/ui/action_button.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/menu_text_scroll.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/window.c
    pebble/apps/system/health/data.c
    pebble/apps/system/health/progress.c
    pebble/apps/system/health/ui.c
    pebble/apps/system/timeline/text_node.c
    pebble/services/activity/health_util.c
    pebble/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/apps/system/health/detail_card.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_chart
  MODULES resources graphics_bitdepth gdraw text_render png time
  SOURCES
    pebble/applib/graphics/text_resources.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    pebble/apps/system/health/hr_chart.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_gbitmap_get_data_row.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
)
