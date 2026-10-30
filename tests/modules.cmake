# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Groups of sources tests pull in with pbl_clar_test(... MODULES ...).

pbl_test_module(time
  SOURCES
    fw/services/clock/time.c
)

pbl_test_module(rand
  SOURCES
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_test_module(pfs
  SOURCES
    fw/services/filesystem/pfs.c
    fw/services/filesystem/flash_translation.c
    fw/flash_region/filesystem_regions.c
    tests/fakes/fake_spi_flash.c
)

pbl_test_module(settings
  REQUIRES pfs
  SOURCES
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
)

pbl_test_module(resources
  REQUIRES pfs
  SOURCES
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/app_file.c
    drivers/flash/flash_crc.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
)

pbl_test_module(gtypes
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/gcolor_definitions.c
)

pbl_test_module(graphics
  REQUIRES gtypes
  SOURCES
    fw/applib/graphics/graphics.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/graphics_circle.c
    fw/applib/graphics/graphics_line.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/framebuffer.c
    fw/applib/graphics/gbitmap.c
)

pbl_test_module(graphics_bitdepth
  REQUIRES graphics
  SOURCES
    fw/applib/graphics/graphics_bitmap.c
    fw/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    fw/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    tests/fakes/fake_display.c
)

pbl_test_module(png
  SOURCES
    fw/applib/graphics/gbitmap_png.c
    third_party/upng/upng/upng.c
    third_party/uzlib/uzlib/src/tinflate.c
    third_party/uzlib/uzlib/src/adler32.c
    third_party/uzlib/uzlib/src/crc32.c
)

pbl_test_module(text
  SOURCES
    fw/applib/graphics/utf8.c
    fw/applib/fonts/codepoint.c
    fw/applib/graphics/arabic_shaping.c
    fw/applib/graphics/bidi.c
    fw/applib/graphics/text_layout.c
)

pbl_test_module(text_render
  REQUIRES text
  SOURCES
    fw/applib/graphics/text_render.c
    fw/applib/graphics/perimeter.c
    tests/fakes/fake_fonts.c
    fw/board/displays/display_gabbro.c
)

pbl_test_module(gdraw
  SOURCES
    fw/applib/graphics/gdraw_command.c
    fw/applib/graphics/gdraw_command_frame.c
    fw/applib/graphics/gdraw_command_image.c
    fw/applib/graphics/gdraw_command_list.c
    fw/applib/graphics/gdraw_command_sequence.c
    fw/applib/graphics/gbitmap_sequence.c
    fw/applib/graphics/gpath.c
    fw/applib/ui/kino/kino_reel.c
    fw/applib/ui/kino/kino_reel_gbitmap.c
    fw/applib/ui/kino/kino_reel_gbitmap_sequence.c
    fw/applib/ui/kino/kino_reel_pdci.c
    fw/applib/ui/kino/kino_reel_pdcs.c
    tests/fakes/fake_applib_resource.c
)

pbl_test_module(timeline_item
  SOURCES
    fw/services/timeline/item.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute.c
)

pbl_test_module(menu_layer
  SOURCES
    fw/applib/ui/menu_layer.c
    fw/applib/ui/scroll_layer.c
    fw/applib/ui/inverter_layer.c
)

pbl_test_module(text_layer
  SOURCES
    fw/applib/ui/text_layer.c
    fw/applib/ui/text_layer_flow.c
)
