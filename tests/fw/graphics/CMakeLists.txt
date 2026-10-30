# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# The bit-depth-agnostic tests are written once and compiled for the
# display depth, with ${BIT_DEPTH_NAME} standing in for it in both the
# fixture paths and the clar suite names.
set(BIT_DEPTH_NAME 8bit)

set(graphics_sources
  fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
  fw/applib/graphics/framebuffer.c
  fw/applib/graphics/gbitmap.c
  fw/applib/graphics/gcolor_definitions.c
  fw/applib/graphics/gtypes.c
  fw/applib/graphics/graphics.c
  fw/applib/graphics/graphics_bitmap.c
  fw/applib/graphics/gtransform.c
  fw/applib/graphics/bitblt.c
  fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
  fw/applib/ui/layer.c
  fw/applib/ui/bitmap_layer.c
  tests/fakes/fake_display.c
  tests/fakes/fake_gbitmap_png.c
  fw/applib/graphics/graphics_private.c
  fw/applib/graphics/graphics_private_raw.c
  fw/applib/graphics/graphics_circle.c
  fw/applib/graphics/graphics_line.c
  fw/applib/graphics/gpath.c
)

set(graphics_templated_tests
  test_framebuffer
  test_graphics_draw_pixel
  test_graphics_draw_stroke
  test_graphics_draw_rect
  test_graphics_fill_rect
  test_graphics_draw_circle
  test_graphics_stroke_circle
  test_graphics_fill_circle
  test_graphics_draw_round_rect
  test_graphics_gpath
  test_graphics_gtransform
  test_graphics_context
  test_blending
  test_graphics_draw_stroke_precise
)

foreach(depth_platform 8bit:emery 1bit:flint)
  string(REPLACE ":" ";" depth_platform ${depth_platform})
  list(GET depth_platform 0 BIT_DEPTH_NAME)
  list(GET depth_platform 1 platform)
  foreach(test ${graphics_templated_tests})
    configure_file(${test}.template.c ${CMAKE_CURRENT_BINARY_DIR}/${test}_${BIT_DEPTH_NAME}.c)
    pbl_clar_test(${test}_${BIT_DEPTH_NAME}
      SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${test}_${BIT_DEPTH_NAME}.c
      SOURCES ${graphics_sources}
      PLATFORMS ${platform}
      INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
      TEST_IMAGES
    )
  endforeach()
endforeach()
set(BIT_DEPTH_NAME 8bit)

set(graphics_draw_text_sources
  fw/applib/graphics/8_bit/framebuffer.c
  fw/applib/graphics/framebuffer.c
  fw/applib/graphics/gcolor_definitions.c
  fw/applib/graphics/gtypes.c
  fw/applib/graphics/gbitmap.c
  fw/applib/graphics/graphics.c
  fw/applib/graphics/gtransform.c
  fw/applib/graphics/bitblt.c
  fw/applib/graphics/8_bit/bitblt_private.c
  fw/applib/ui/layer.c
  tests/fakes/fake_gbitmap_get_data_row.c
  tests/fakes/fake_gbitmap_png.c
  tests/fakes/fake_display.c
  fw/applib/graphics/text_render.c
  fw/applib/graphics/utf8.c
  fw/applib/graphics/text_layout.c
  fw/applib/graphics/bidi.c
  fw/applib/graphics/arabic_shaping.c
  fw/applib/graphics/perimeter.c
  fw/applib/graphics/text_resources.c
  fw/flash_region/flash_region.c
  fw/flash_region/filesystem_regions.c
  fw/system/hexdump.c
  fw/applib/fonts/codepoint.c
  fw/resource/resource.c
  fw/resource/resource_storage.c
  fw/resource/resource_storage_builtin.c
  fw/resource/resource_storage_file.c
  fw/resource/resource_storage_flash.c
  subsys/fs/pfs/flash_translation.c
  subsys/fs/pfs/pfs.c
  subsys/fs/fs.c
  subsys/fs/pfs/pfs_fs.c
  tests/fakes/fake_pfs.c
  fw/services/process_management/app_file.c
  drivers/flash/flash_crc.c
  tests/fakes/fake_fonts.c
  tests/fakes/fake_rtc.c
  tests/fakes/fake_spi_flash.c
  tests/fixtures/resources/builtin_resources.auto.c
  tests/fixtures/resources/pfs_resource_table.c
  fw/applib/graphics/graphics_private.c
  fw/applib/graphics/graphics_private_raw.c
  fw/applib/graphics/graphics_circle.c
  fw/applib/graphics/graphics_line.c
)

configure_file(test_graphics_draw_text.template.c
               ${CMAKE_CURRENT_BINARY_DIR}/test_graphics_draw_text_${BIT_DEPTH_NAME}.c)
pbl_clar_test(test_graphics_draw_text_${BIT_DEPTH_NAME}
  SOURCE ${CMAKE_CURRENT_BINARY_DIR}/test_graphics_draw_text_${BIT_DEPTH_NAME}.c
  SOURCES ${graphics_draw_text_sources}
  PLATFORMS emery
  OVERRIDES dummy_board
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  TEST_IMAGES
)

pbl_clar_test(test_graphics_draw_bitmap
  MODULES graphics png
  SOURCES
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/ui/bitmap_layer.c
    fw/applib/ui/layer.c
  PLATFORMS emery flint
  TEST_IMAGES
)

pbl_clar_test(test_graphics_draw_text_flow
  MODULES resources graphics text_render
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/gtransform.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/ui/layer.c
    tests/fakes/fake_gbitmap_get_data_row.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_display.c
    fw/applib/graphics/text_resources.c
    fw/system/hexdump.c
    tests/fakes/fake_rtc.c
  PLATFORMS gabbro
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_graphics_colors
  MODULES gtypes
)

pbl_clar_test(test_gtypes
  SOURCES
    fw/applib/graphics/gtypes.c
  PLATFORMS emery gabbro
)

pbl_clar_test(test_graphics_circle
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/graphics_circle.c
)

pbl_clar_test(test_gbitmap_data_row_info
  SOURCES
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_resource_syscalls.c
    fw/applib/graphics/gbitmap.c
)

pbl_clar_test(test_gbitmap_resource_validation
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_applib_resource.c
)

pbl_clar_test(test_gbitmap_formats
  SOURCES
    fw/applib/graphics/gtypes.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_resource_syscalls.c
    fw/applib/graphics/gbitmap.c
    fw/board/displays/display_gabbro.c
  PLATFORMS emery gabbro
)

pbl_clar_test(test_gbitmap_processor
  MODULES gtypes
  SOURCES
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/graphics.c
    fw/applib/graphics/graphics_bitmap.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_applib_resource.c
    tests/stubs/stubs_bitblt.c
)

pbl_clar_test(test_graphics_blending
  MODULES gtypes
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/graphics.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/8_bit/bitblt_private.c
  PLATFORMS emery
)

pbl_clar_test(test_gdraw_command
  SOURCES
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_frame.c
)

pbl_clar_test(test_gdraw_command_sequence
  SOURCES
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_sequence.c
)

pbl_clar_test(test_gdraw_command_resources
  MODULES gtypes
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_sequence.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_resource_syscalls.c
    tests/fakes/fake_applib_resource.c
  TEST_IMAGES
)

pbl_clar_test(test_gdraw_command_transforms
  MODULES graphics
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/gpath.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/ui/animation_interpolate.c
    fw/applib/ui/animation_timing.c
    tests/fakes/fake_gbitmap_png.c
    tests/fw/graphics/weather_app_resources.c
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_transforms.c
  OVERRIDES dummy_board
  TEST_IMAGES
)

pbl_clar_test(test_pdc
  MODULES graphics png
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  TEST_IMAGES
)

pbl_clar_test(test_png
  MODULES graphics png
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  TEST_IMAGES
)

pbl_clar_test(test_gbitmap_sequence
  MODULES graphics png
  SOURCES
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/board/displays/display_gabbro.c
    tests/fakes/fake_resource_syscalls.c
  PLATFORMS gabbro
  TEST_IMAGES
)

pbl_clar_test(test_pbi
  MODULES graphics
  SOURCES
    third_party/upng/upng/upng.c
    third_party/uzlib/uzlib/src/tinflate.c
    third_party/uzlib/uzlib/src/adler32.c
    third_party/uzlib/uzlib/src/crc32.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  TEST_IMAGES
)

pbl_clar_test(test_palettized_conversion
  MODULES graphics png
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  TEST_IMAGES
)

pbl_clar_test(test_graphics_text_node
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/apps/system/timeline/text_node.c
    tests/stubs/stubs_text_layout.c
)

pbl_clar_test(test_perimeter
  SOURCES
    fw/applib/graphics/perimeter.c
    fw/applib/graphics/gtypes.c
  PLATFORMS emery gabbro
  TEST_IMAGES
)

pbl_clar_test(test_bitblt_palette_1bit
  MODULES graphics png
  SOURCES
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
  PLATFORMS flint
  TEST_IMAGES
)

pbl_clar_test(test_framebuffer_duma
  MODULES graphics png
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/board/displays/display_gabbro.c
  PLATFORMS emery gabbro
  TEST_IMAGES
)

pbl_clar_test(test_framebuffer_legacy
  MODULES graphics png
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/board/displays/display_gabbro.c
  PLATFORMS gabbro
  TEST_IMAGES
)

pbl_clar_test(test_graphics_draw_core
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_gbitmap_get_data_row.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  PLATFORMS emery
  TEST_IMAGES
)

pbl_clar_test(test_graphics_draw_implementation
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_gbitmap_get_data_row.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/graphics/8_bit/bitblt_private.c
  TEST_IMAGES
)

pbl_clar_test(test_bitblt
  MODULES graphics
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/graphics/gtransform.c
    fw/applib/ui/layer.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_display.c
  TEST_IMAGES
)

pbl_clar_test(test_bitblt_palette
  MODULES graphics
  SOURCES
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/graphics/gtransform.c
    fw/applib/ui/layer.c
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_display.c
  TEST_IMAGES
)

# Skipped for now; see pbl_test_broken() in tests/CMakeLists.txt.
pbl_clar_test(test_graphics_draw_line
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
  PLATFORMS emery gabbro
  TEST_IMAGES
)

# Skipped for now; see pbl_test_broken() in tests/CMakeLists.txt.
pbl_clar_test(test_graphics_draw_rotated_bitmap
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    tests/fakes/fake_gbitmap_get_data_row.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    fw/applib/graphics/graphics_bitmap.c
  PLATFORMS emery gabbro
  TEST_IMAGES
)

pbl_clar_test(test_raw_image)
