# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_music
  MODULES resources graphics_bitdepth text_render png text_layer
  SOURCES
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_sequence.c
    fw/applib/graphics/gpath.c
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/text_resources.c
    fw/applib/ui/action_bar_layer.c
    fw/applib/ui/status_bar_layer.c
    fw/applib/ui/layer.c
    fw/system/hexdump.c
    tests/fakes/fake_applib_resource.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_animation.c
    tests/stubs/stubs_system_theme.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/bitmap_layer.c
    fw/applib/ui/progress_layer.c
    fw/applib/ui/window.c
    fw/services/imaging/resident_image.c
  PLATFORMS flint emery gabbro
  DEFINES USE_DISPLAY_PERIMETER_ON_FONT_LAYOUT=1
  OVERRIDES dummy_board
  TEST_IMAGES
)
