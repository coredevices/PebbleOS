# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_app_message
  SOURCES
    fw/applib/app_message/app_message.c
    fw/applib/app_message/app_message_outbox.c
    fw/applib/app_message/app_message_inbox.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/process_management/pebble_process_info.c
    fw/applib/dict.c
)

pbl_clar_test(test_app_inbox
  SOURCES
    fw/applib/app_inbox.c
    fw/services/app_inbox_service/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_outbox
  SOURCES
    fw/applib/app_outbox.c
    fw/services/app_outbox_service/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_cpu_cache
  SOURCES
    fw/applib/cpu_cache.c
    kernel/arch/arm/cache.c
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_persist
  SOURCES
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/applib/dict.c
    fw/applib/persist.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/persist/service.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_pbl_std
  SOURCES
    fw/flash_region/flash_region.c
    fw/applib/pbl_std/pbl_std.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_strftime
  SOURCES
    fw/flash_region/flash_region.c
    fw/applib/pbl_std/pbl_std.c
    fw/applib/pbl_std/strftime.c
    fw/applib/pbl_std/timelocal.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_text_render
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/text_render.c
    fw/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_text_resources
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/system/hexdump.c
    fw/applib/fonts/codepoint.c
    fw/applib/fonts/fonts.c
    fw/applib/graphics/text_resources.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    fw/drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
    tests/fw/applib/test_text_resources_font_stub.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_bitmap_layer
  SOURCES
    fw/applib/ui/bitmap_layer.c
    fw/applib/ui/layer.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gtypes.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_unobstructed_area_service
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/ui/layer.c
    fw/applib/unobstructed_area_service.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_touch_service
  SOURCES
    fw/applib/touch_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance
  SOURCES
    fw/applib/app_glance.c
    fw/applib/template_string.c
    fw/services/app_glances/app_glance_service.c
    fw/services/blob_db/app_glance_db.c
    fw/services/timeline/attribute.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_template_string
  SOURCES
    fw/applib/template_string.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_dict
  SOURCES
    fw/applib/dict.c
)

pbl_clar_test(test_qr_code
  SOURCES
    fw/applib/graphics/gtypes.c
    fw/applib/graphics/gbitmap.c
    tests/fakes/fake_gbitmap_png.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/graphics/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/ui/qr_code.c
    fw/applib/graphics/8_bit/bitblt_private.c
    fw/applib/graphics/bitblt.c
    fw/applib/graphics/graphics_private.c
    fw/applib/graphics/graphics_private_raw.c
    fw/applib/graphics/graphics_circle.c
    fw/applib/graphics/graphics_line.c
    fw/applib/graphics/graphics.c
    third_party/qr_code_generator/QR-Code-generator/c/qrcodegen.c
    third_party/qr_code_generator/qrcodegen_ext.c
  INCLUDES
    tests/fw/graphics
    third_party/qr_code_generator/include
    third_party/qr_code_generator/QR-Code-generator/c
  DEFINES QRCODEGEN_TEST
  PLATFORMS obelix
  TEST_IMAGES
)
