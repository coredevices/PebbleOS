# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# The timezone database the tests check against, as a C array. The
# firmware bakes the same bytes into a resource.
set(timezone_fixture ${CMAKE_CURRENT_BINARY_DIR}/timezone_fixture.auto.h)
set(timezones_olson ${PBL_BASE}/resources/normal/base/tzdata/timezones_olson.txt)
add_custom_command(
  OUTPUT ${timezone_fixture}
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/resources.py tzdata-header
          --input ${timezones_olson} --output ${timezone_fixture}
  DEPENDS ${timezones_olson} ${PBL_BASE}/tools/cmake/resources.py
  COMMENT "Generating timezone test fixture"
  VERBATIM
)
add_custom_target(pbl_timezone_fixture DEPENDS ${timezone_fixture})

pbl_clar_test(test_pfs
  SOURCES
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/system/hexdump.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_rtc.c
  PLATFORMS obelix
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)

pbl_clar_test(test_put_bytes
  SOURCES
    fw/services/put_bytes/put_bytes.c
    fw/services/put_bytes/put_bytes_storage.c
    tests/fakes/fake_session.c
    tests/fakes/fake_events.c
    tests/fakes/fake_put_bytes_storage_mem.c
    tests/fakes/fake_resource_storage.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS obelix gabbro
  OVERRIDES dummy_board
)

pbl_clar_test(test_tick_timer
  SOURCES
    fw/services/tick_timer/service.c
)

pbl_clar_test(test_cron
  SOURCES
    tests/fakes/fake_rtc.c
    subsys/cron/cron.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
)

pbl_clar_test(test_contacts
  SOURCES
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/contacts/attributes_address.c
    fw/services/contacts/contacts.c
    fw/services/blob_db/contacts_db.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attribute.c
    fw/kernel/rand.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_evented_timer
  SOURCES
    fw/services/evented_timer/service.c
)

pbl_clar_test(test_music_endpoint
  SOURCES
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
    tests/fakes/fake_rtc.c
    fw/services/music/endpoint.c
    fw/services/music/service.c
)

pbl_clar_test(test_imaging
  SOURCES
    tests/fakes/fake_session.c
    fw/services/imaging/imaging.c
)

pbl_clar_test(test_analytics_native
  SOURCES
    fw/services/analytics/native.c
)

pbl_clar_test(test_compass_cal
  SOURCES
    fw/services/ecompass/correction.c
)

pbl_clar_test(test_light
  SOURCES
    fw/services/light/service.c
  OVERRIDES getafix_backlight
)

pbl_clar_test(test_als_screen_compensation
  SOURCES
    fw/services/light/als_screen_compensation_math.c
    fw/services/light/als_screen_compensation_region.c
  DEFINES CONFIG_ALS_SCREEN_COMPENSATION CONFIG_AMBIENT_LIGHT_BITS=16
  OVERRIDES dummy_board
)

pbl_clar_test(test_phone_pp
  SOURCES
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
    fw/services/phone_call/util.c
    fw/services/phone_pp/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_phone_call
  SOURCES
    fw/services/phone_call/service.c
)

pbl_clar_test(test_regular_timer
  SOURCES
    fw/services/regular_timer/service.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_debounced_connection_service
  SOURCES
    fw/services/regular_timer/service.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_session.c
    fw/services/debounced_connection_service/service.c
)

pbl_clar_test(test_timeline_item
  SOURCES
    tests/fakes/fake_rtc.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attribute.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
)

pbl_clar_test(test_clock
  SOURCES
    fw/services/i18n/i18n.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    tests/fakes/fake_bootbits.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_events.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
    fw/services/timezone_database/service.c
    fw/services/clock/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timezone_database
  SOURCES
    tests/fakes/fake_spi_flash.c
    fw/services/timezone_database/service.c
  OVERRIDES test_timezone  DEPENDS pbl_timezone_fixture
)

pbl_clar_test(test_wakeup
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/process_management/pebble_process_info.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/wakeup/service.c
    tests/fakes/fake_bootbits.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_migrate_wakeup
  SOURCES
    drivers/flash/flash_crc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/process_management/pebble_process_info.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/wakeup/service.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_registry_endpoint
  SOURCES
    fw/services/registry_endpoint/service.c
)

pbl_clar_test(test_audio_endpoint
  SOURCES
    fw/services/audio_endpoint/service.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_speaker_service
  SOURCES
    fw/services/speaker/speaker_service.c
    fw/services/speaker/note_sequence.c
    fw/services/speaker/pcm_stream.c
    fw/services/speaker/track_player.c
    tests/fakes/fake_events.c
  DEFINES CONFIG_SPEAKER=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_transcription
  SOURCES
    fw/services/voice/transcription.c
)

pbl_clar_test(test_voice_endpoint
  SOURCES
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/voice/transcription.c
    fw/services/voice_endpoint/service.c
    tests/fakes/fake_session.c
  DEFINES CONFIG_MIC
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_cache
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/app_cache/service.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_install_manager
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/app_file.c
    fw/services/app_cache/service.c
    fw/services/process_management/app_storage.c
    fw/process_management/app_install_manager.c
    fw/process_management/pebble_process_md.c
    fw/services/blob_db/app_db.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gtypes.c
    fw/process_management/pebble_process_info.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fakes/fake_system_app_registry_apps.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board fake_app_registry
)

pbl_clar_test(test_app_menu_data_source
  SOURCES
    fw/applib/graphics/gcolor_definitions.c
    fw/applib/graphics/gtypes.c
    drivers/flash/flash_crc.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    fw/process_management/app_install_manager.c
    fw/process_management/app_menu_data_source.c
    fw/process_management/pebble_process_info.c
    fw/process_management/pebble_process_md.c
    fw/resource/resource.c
    fw/resource/resource_storage.c
    fw/resource/resource_storage_builtin.c
    fw/resource/resource_storage_file.c
    fw/resource/resource_storage_flash.c
    fw/services/app_cache/service.c
    fw/services/blob_db/app_db.c
    fw/services/filesystem/app_file.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/process_management/app_order_storage.c
    fw/services/process_management/app_storage.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_system_app_registry_apps.c
    tests/fixtures/resources/builtin_resources.auto.c
  OVERRIDES dummy_board fake_app_registry
)

pbl_clar_test(test_process_loader_storage
  SOURCES
    lib/util/build_id.c
    fw/kernel/util/segment.c
    fw/process_management/pebble_process_info.c
    fw/process_management/pebble_process_md.c
    fw/services/process_management/app_storage.c
    fw/services/process_management/process_loader_storage.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_fetch_endpoint
  SOURCES
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/services/app_fetch_endpoint/service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_manager
  SOURCES
    fw/services/accel_manager/service.c
    tests/fakes/fake_events.c
  DEFINES CONFIG_SERVICE_ACCEL_MANAGER_BATCH_SAMPLES=25
  OVERRIDES dummy_board
)

pbl_clar_test(test_shared_prf_storage_v3
  SOURCES
    fw/services/shared_prf_storage/shared_prf_storage.c
    tests/fakes/fake_flash_region.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS obelix
  OVERRIDES shared_prf_storage_v3
)

pbl_clar_test(test_app_file
  SOURCES
    fw/services/filesystem/app_file.c
)

pbl_clar_test(test_do_not_disturb
  SOURCES
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/notifications/alerts_preferences.c
    fw/services/notifications/do_not_disturb.c
    subsys/cron/cron.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_weather_service
  SOURCES
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    fw/services/blob_db/weather_db.c
    fw/services/weather/weather_service.c
    tests/fw/services/blob_db/weather_data_shared.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_vibe
  SOURCES
    fw/services/vibe_pattern/service.c
    fw/applib/ui/vibes.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_vibe_intensity
  SOURCES
    fw/services/vibes/vibe_intensity.c
)

pbl_clar_test(test_vibe_score
  SOURCES
    fw/services/vibes/vibe_score.c
)

pbl_clar_test(test_vibe_score_info
  SOURCES
    fw/services/vibes/vibe_score_info.c
)

pbl_clar_test(test_touch
  SOURCES
    fw/services/touch/touch.c
    tests/fakes/fake_events.c
)

pbl_clar_test(test_touch_session
  SOURCES
    fw/services/touch/touch_session.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_hrm_manager
  SOURCES
    fw/services/hrm/hrm_manager.c
    kernel/tick.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_accel_service.c
  DEFINES CONFIG_HRM_HRV
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance_service
  SOURCES
    fw/services/app_glances/app_glance_service.c
    fw/services/blob_db/app_glance_db.c
    fw/services/timeline/attribute.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings_file.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_clock_time
  SOURCES
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    lib/c/time/mktime.c
    tests/fakes/fake_rtc.c
)
