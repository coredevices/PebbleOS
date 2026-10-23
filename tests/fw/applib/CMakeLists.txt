# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_app_message
  MODULES rand
  SOURCES
    fw/applib/app_message/app_message.c
    fw/applib/app_message/app_message_outbox.c
    fw/applib/app_message/app_message_inbox.c
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
  MODULES settings rand
  SOURCES
    fw/applib/dict.c
    fw/applib/persist.c
    fw/services/filesystem/app_file.c
    fw/services/persist/service.c
    tests/fakes/fake_rtc.c
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
  MODULES gtypes
  SOURCES
    fw/applib/graphics/gbitmap.c
    fw/applib/graphics/text_render.c
    fw/applib/fonts/codepoint.c
    tests/fakes/fake_gbitmap_png.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_text_resources
  MODULES resources
  SOURCES
    fw/system/hexdump.c
    fw/applib/fonts/codepoint.c
    fw/applib/fonts/fonts.c
    fw/applib/graphics/text_resources.c
    tests/fakes/fake_rtc.c
    tests/fw/applib/test_text_resources_font_stub.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_system_resource_language_fonts
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
    fw/resource/system_resource.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
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
    fw/applib/ui/bitmap_layer.c
    fw/applib/ui/layer.c
    tests/fakes/fake_gbitmap_png.c
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

pbl_clar_test(test_notification_service
  SOURCES
    fw/applib/notification_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_service
  SOURCES
    fw/applib/accel_service.c
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
  MODULES graphics
  SOURCES
    tests/fakes/fake_gbitmap_png.c
    fw/applib/graphics/8_bit/framebuffer.c
    fw/applib/ui/layer.c
    fw/applib/ui/qr_code.c
    fw/applib/graphics/8_bit/bitblt_private.c
    third_party/qr_code_generator/QR-Code-generator/c/qrcodegen.c
    third_party/qr_code_generator/qrcodegen_ext.c
  INCLUDES
    tests/fw/graphics
    third_party/qr_code_generator/include
    third_party/qr_code_generator/QR-Code-generator/c
  DEFINES QRCODEGEN_TEST
  PLATFORMS emery
  TEST_IMAGES
)
