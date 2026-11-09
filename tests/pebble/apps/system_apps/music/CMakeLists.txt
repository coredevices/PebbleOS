# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_music
  MODULES resources graphics_bitdepth text_render png text_layer
  SOURCES
    pebble/applib/graphics/gbitmap_sequence.c
    pebble/applib/graphics/gdraw_command.c
    pebble/applib/graphics/gdraw_command_frame.c
    pebble/applib/graphics/gdraw_command_image.c
    pebble/applib/graphics/gdraw_command_list.c
    pebble/applib/graphics/gdraw_command_sequence.c
    pebble/applib/graphics/gpath.c
    pebble/applib/graphics/gtransform.c
    pebble/applib/graphics/text_resources.c
    pebble/applib/ui/action_bar_layer.c
    pebble/applib/ui/status_bar_layer.c
    pebble/applib/ui/layer.c
    subsys/logging/hexdump.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    pebble/applib/ui/animation_interpolate.c
    pebble/applib/ui/bitmap_layer.c
    pebble/applib/ui/progress_layer.c
    pebble/applib/ui/window.c
  PLATFORMS flint emery gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
