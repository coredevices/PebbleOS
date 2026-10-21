# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_weather_app_layout
  MODULES resources graphics_bitdepth gdraw text_render png text_layer
  SOURCES
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/layer.c
    fw/apps/system/timeline/text_node.c
    fw/system/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/kino/kino_layer.c
    fw/applib/ui/kino/kino_player.c
    fw/applib/ui/kino/kino_reel_custom.c
    fw/applib/ui/kino/kino_reel/transform.c
    fw/applib/ui/window.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/inverter_layer.c
    fw/applib/ui/content_indicator.c
    fw/applib/ui/shadows.c
    fw/applib/ui/bitmap_layer.c
    fw/applib/graphics/gdraw_command_transforms.c
    fw/apps/system/weather/weather_app_layout.c
    fw/apps/system/weather/weather_math.c
    fw/services/timeline/timeline_resources.c
    fw/apps/system/weather/weather_types.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    fw/services/clock/time.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
