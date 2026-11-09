# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_app_message
  MODULES rand
  SOURCES
    pebble/applib/app_message/app_message.c
    pebble/applib/app_message/app_message_outbox.c
    pebble/applib/app_message/app_message_inbox.c
    pebble/process_management/pebble_process_info.c
    pebble/applib/dict.c
)

pbl_clar_test(test_app_inbox
  SOURCES
    pebble/applib/app_inbox.c
    pebble/services/app_inbox_service/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_outbox
  SOURCES
    pebble/applib/app_outbox.c
    pebble/services/app_outbox_service/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_cpu_cache
  SOURCES
    pebble/applib/cpu_cache.c
    kernel/arch/arm/cache.c
  DEFINES UNITTEST_WITH_SYSCALL_PRIVILEGES=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_persist
  MODULES settings rand
  SOURCES
    pebble/applib/dict.c
    pebble/applib/persist.c
    pebble/services/process_management/app_file.c
    pebble/services/persist/service.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/builtin_resources.auto.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_pbl_std
  SOURCES
    pebble/applib/pbl_std/pbl_std.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_strftime
  SOURCES
    pebble/applib/pbl_std/pbl_std.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_text_render
  MODULES gtypes
  SOURCES
    pebble/applib/graphics/gbitmap.c
    pebble/applib/graphics/text_render.c
    pebble/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_text_resources
  MODULES resources
  SOURCES
    subsys/logging/hexdump.c
    pebble/applib/fonts/codepoint.c
    pebble/applib/fonts/fonts.c
    pebble/applib/graphics/text_resources.c
    tests/fakes/fake_rtc.c
    tests/pebble/applib/test_text_resources_font_stub.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_system_resource_language_fonts
  SOURCES
    subsys/logging/hexdump.c
    pebble/applib/fonts/codepoint.c
    pebble/applib/fonts/fonts.c
    pebble/applib/graphics/text_resources.c
    pebble/resource/resource.c
    pebble/resource/resource_storage.c
    pebble/resource/resource_storage_builtin.c
    pebble/resource/resource_storage_file.c
    pebble/resource/resource_storage_flash.c
    pebble/resource/system_resource.c
    subsys/fs/pfs/flash_translation.c
    subsys/fs/pfs/pfs.c
    subsys/fs/fs.c
    subsys/fs/pfs/pfs_fs.c
    tests/fakes/fake_pfs.c
    pebble/services/process_management/app_file.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
  OVERRIDES dummy_board language_fonts
)

pbl_clar_test(test_bitmap_layer
  MODULES gtypes
  SOURCES
    pebble/applib/ui/bitmap_layer.c
    pebble/applib/ui/layer.c
    tests/fakes/fake_gbitmap_png.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_unobstructed_area_service
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/applib/ui/layer.c
    pebble/applib/unobstructed_area_service.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_touch_service
  SOURCES
    pebble/applib/touch_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_dark_mode_service
  SOURCES
    pebble/applib/dark_mode_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_service
  SOURCES
    pebble/applib/accel_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance
  SOURCES
    pebble/applib/app_glance.c
    pebble/applib/template_string.c
    pebble/services/app_glances/app_glance_service.c
    pebble/services/blob_db/app_glance_db.c
    pebble/services/timeline/attribute.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_template_string
  SOURCES
    pebble/applib/template_string.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_dict
  SOURCES
    pebble/applib/dict.c
)

pbl_clar_test(test_qr_code
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    pebble/applib/graphics/8_bit/framebuffer.c
    pebble/applib/ui/layer.c
    pebble/applib/ui/qr_code.c
    pebble/applib/graphics/8_bit/bitblt_private.c
    third_party/qr_code_generator/QR-Code-generator/c/qrcodegen.c
    third_party/qr_code_generator/qrcodegen_ext.c
  INCLUDES
    tests/pebble/graphics
    third_party/qr_code_generator/include
    third_party/qr_code_generator/QR-Code-generator/c
  DEFINES QRCODEGEN_TEST
  PLATFORMS emery
  TEST_IMAGES
)
