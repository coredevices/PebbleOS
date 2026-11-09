# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Groups of sources tests pull in with pbl_clar_test(... MODULES ...).

pbl_test_module(time
  SOURCES
    pebble/services/clock/time.c
)

pbl_test_module(rand
  SOURCES
    pebble/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_test_module(pfs
  SOURCES
    subsys/fs/pfs/pfs.c
    subsys/fs/fs.c
    subsys/fs/pfs/pfs_fs.c
    tests/fakes/fake_pfs.c
    subsys/fs/pfs/flash_translation.c
    tests/fakes/fake_spi_flash.c
)

pbl_test_module(settings
  REQUIRES pfs
  SOURCES
    subsys/settings/settings.c
    subsys/settings/raw_iter.c
)

pbl_test_module(resources
  REQUIRES pfs
  SOURCES
    pebble/resource/resource.c
    pebble/resource/resource_storage.c
    pebble/resource/resource_storage_builtin.c
    pebble/resource/resource_storage_file.c
    pebble/resource/resource_storage_flash.c
    pebble/services/process_management/app_file.c
    drivers/flash/flash_crc.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
)

pbl_test_module(gtypes
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/graphics/gcolor_definitions.c
)

pbl_test_module(graphics
  REQUIRES gtypes
  SOURCES
    pebble/applib/graphics/graphics.c
    pebble/applib/graphics/graphics_private.c
    pebble/applib/graphics/graphics_private_raw.c
    pebble/applib/graphics/graphics_circle.c
    pebble/applib/graphics/graphics_line.c
    pebble/applib/graphics/bitblt.c
    pebble/applib/graphics/framebuffer.c
    pebble/applib/graphics/gbitmap.c
)

pbl_test_module(graphics_bitdepth
  REQUIRES graphics
  SOURCES
    pebble/applib/graphics/graphics_bitmap.c
    pebble/applib/graphics/@BITDEPTH@_bit/bitblt_private.c
    pebble/applib/graphics/@BITDEPTH@_bit/framebuffer.c
    tests/fakes/fake_display.c
)

pbl_test_module(png
  SOURCES
    pebble/applib/graphics/gbitmap_png.c
    third_party/upng/upng/upng.c
    third_party/uzlib/uzlib/src/tinflate.c
    third_party/uzlib/uzlib/src/adler32.c
    third_party/uzlib/uzlib/src/crc32.c
)

pbl_test_module(text
  SOURCES
    pebble/applib/graphics/utf8.c
    pebble/applib/fonts/codepoint.c
    pebble/applib/graphics/arabic_shaping.c
    pebble/applib/graphics/bidi.c
    pebble/applib/graphics/text_layout.c
)

pbl_test_module(text_render
  REQUIRES text
  SOURCES
    pebble/applib/graphics/text_render.c
    pebble/applib/graphics/perimeter.c
    tests/fakes/fake_fonts.c
    tests/fakes/fake_round_display.c
)

pbl_test_module(gdraw
  SOURCES
    pebble/applib/graphics/gdraw_command.c
    pebble/applib/graphics/gdraw_command_frame.c
    pebble/applib/graphics/gdraw_command_image.c
    pebble/applib/graphics/gdraw_command_list.c
    pebble/applib/graphics/gdraw_command_sequence.c
    pebble/applib/graphics/gbitmap_sequence.c
    pebble/applib/graphics/gpath.c
    pebble/applib/ui/kino/kino_reel.c
    pebble/applib/ui/kino/kino_reel_gbitmap.c
    pebble/applib/ui/kino/kino_reel_gbitmap_sequence.c
    pebble/applib/ui/kino/kino_reel_pdci.c
    pebble/applib/ui/kino/kino_reel_pdcs.c
    tests/fakes/fake_applib_resource.c
)

pbl_test_module(timeline_item
  SOURCES
    pebble/services/timeline/item.c
    pebble/services/timeline/attribute_group.c
    pebble/services/timeline/attributes_actions.c
    pebble/services/timeline/attribute.c
)

pbl_test_module(menu_layer
  SOURCES
    pebble/applib/ui/menu_layer.c
    pebble/applib/ui/scroll_layer.c
    pebble/applib/ui/inverter_layer.c
)

pbl_test_module(text_layer
  SOURCES
    pebble/applib/ui/text_layer.c
    pebble/applib/ui/text_layer_flow.c
)
