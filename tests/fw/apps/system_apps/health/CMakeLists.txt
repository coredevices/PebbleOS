# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_health_card_view
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/activity_detail_card.c
    fw/apps/system/health/activity_summary_card.c
    fw/apps/system/health/hr_detail_card.c
    fw/apps/system/health/hr_chart.c
    fw/apps/system/health/hr_summary_card.c
    fw/apps/system/health/sleep_detail_card.c
    fw/apps/system/health/sleep_summary_card.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/card_view.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_activity_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/activity_detail_card.c
    fw/apps/system/health/activity_summary_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_sleep_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/sleep_detail_card.c
    fw/apps/system/health/sleep_summary_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_summary_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/hr_detail_card.c
    fw/apps/system/health/hr_chart.c
    fw/apps/system/health/hr_summary_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_activity_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/activity_detail_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_sleep_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/sleep_detail_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/apps/system/health/hr_detail_card.c
    fw/apps/system/health/hr_chart.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_detail_card
  MODULES resources graphics_bitdepth gdraw text_render png menu_layer time text_layer
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
    fw/applib/graphics/gpath_builder.c
    fw/applib/ui/action_button.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/menu_text_scroll.c
    fw/applib/ui/shadows.c
    fw/applib/ui/window.c
    fw/apps/system/health/data.c
    fw/apps/system/health/progress.c
    fw/apps/system/health/ui.c
    fw/apps/system/timeline/text_node.c
    fw/services/activity/health_util.c
    fw/services/timeline/timeline_resources.c
    tests/fakes/fake_clock.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/apps/system/health/detail_card.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_health_hr_chart
  MODULES resources graphics_bitdepth gdraw text_render png time
  SOURCES
    fw/applib/graphics/text_resources.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    fw/apps/system/health/hr_chart.c
    tests/fakes/fake_clock.c
    tests/fakes/fake_gbitmap_get_data_row.c
  PLATFORMS obelix gabbro asterix
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
)
