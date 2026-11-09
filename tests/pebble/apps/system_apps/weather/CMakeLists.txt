# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_weather_app_layout
  MODULES resources graphics_bitdepth gdraw text_render png text_layer
  SOURCES
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/layer.c
    pebble/apps/system/timeline/text_node.c
    subsys/logging/hexdump.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/kino/kino_layer.c
    pebble/applib/ui/kino/kino_player.c
    pebble/applib/ui/kino/kino_reel_custom.c
    pebble/applib/ui/kino/kino_reel/transform.c
    pebble/applib/ui/window.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/inverter_layer.c
    pebble/applib/ui/content_indicator.c
    pebble/applib/ui/shadows.c
    pebble/applib/ui/bitmap_layer.c
    pebble/applib/graphics/gdraw_command_transforms.c
    pebble/apps/system/weather/weather_app_layout.c
    pebble/apps/system/weather/weather_math.c
    pebble/services/timeline/timeline_resources.c
    pebble/apps/system/weather/weather_types.c
    tests/fixtures/resources/timeline_resource_table.auto.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    pebble/services/clock/time.c
  PLATFORMS emery gabbro flint
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
