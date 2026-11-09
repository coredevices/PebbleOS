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

pbl_clar_test(test_language_pack_protocol
  SOURCES pebble/kernel/language_pack_protocol.c
)

pbl_clar_test(test_put_bytes
  SOURCES
    pebble/services/put_bytes/put_bytes.c
    pebble/services/put_bytes/put_bytes_storage.c
    tests/fakes/fake_session.c
    tests/fakes/fake_events.c
    tests/fakes/fake_put_bytes_storage_mem.c
    tests/fakes/fake_resource_storage.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS emery gabbro flint
  OVERRIDES dummy_board
)

pbl_clar_test(test_get_bytes
  SOURCES
    pebble/services/get_bytes/get_bytes.c
    tests/fakes/fake_session.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_SOC_POSIX
)

pbl_clar_test(test_language_pack
  MODULES resources
  SOURCES
    subsys/i18n/i18n.c
    pebble/services/language_pack/language_pack.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_language_pack_storage
  SOURCES
    pebble/services/put_bytes/put_bytes_storage_file.c
    subsys/i18n/i18n.c
    pebble/services/language_pack/language_pack.c
    pebble/resource/resource.c
    pebble/resource/resource_storage.c
    pebble/resource/resource_storage_builtin.c
    pebble/resource/resource_storage_file.c
    pebble/resource/resource_storage_flash.c
    subsys/fs/pfs/flash_translation.c
    subsys/fs/pfs/pfs.c
    subsys/fs/fs.c
    subsys/fs/pfs/pfs_fs.c
    tests/fakes/fake_pfs.c
    pebble/services/process_management/app_file.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fixtures/resources/pfs_resource_table.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_tick_timer
  SOURCES
    pebble/services/tick_timer/service.c
)

pbl_clar_test(test_cron
  MODULES time
  SOURCES
    tests/fakes/fake_rtc.c
    subsys/cron/cron.c
)

pbl_clar_test(test_contacts
  MODULES settings timeline_item time rand
  SOURCES
    tests/fakes/fake_rtc.c
    pebble/services/contacts/attributes_address.c
    pebble/services/contacts/contacts.c
    pebble/services/blob_db/contacts_db.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_evented_timer
  SOURCES
    pebble/services/evented_timer/service.c
)

pbl_clar_test(test_music_endpoint
  SOURCES
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
    tests/fakes/fake_rtc.c
    pebble/services/music/endpoint.c
    pebble/services/music/service.c
)

pbl_clar_test(test_ping
  SOURCES
    pebble/services/ping/service.c
)

pbl_clar_test(test_imaging
  SOURCES
    tests/fakes/fake_session.c
    pebble/services/imaging/imaging.c
)

pbl_clar_test(test_compass_cal
  SOURCES
    pebble/services/ecompass/correction.c
)

pbl_clar_test(test_light
  SOURCES
    pebble/services/light/service.c
    subsys/cron/cron.c
    tests/fakes/fake_rtc.c
  MODULES time
  DEFINES
    CONFIG_BACKLIGHT_HAS_COLOR
    CONFIG_ALS_SCREEN_COMPENSATION
    CONFIG_SERVICE_LIGHT_BACKLIGHT_ON_PERCENT=25
    CONFIG_SERVICE_LIGHT_DARK_THRESHOLD=0
    CONFIG_SERVICE_LIGHT_DARK_THRESHOLD_DELTA=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_als_screen_compensation
  SOURCES
    pebble/services/light/als_screen_compensation_math.c
    pebble/services/light/als_screen_compensation_region.c
  DEFINES CONFIG_ALS_SCREEN_COMPENSATION CONFIG_AMBIENT_LIGHT_BITS=16
  OVERRIDES dummy_board
)

pbl_clar_test(test_phone_pp
  SOURCES
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
    pebble/services/phone_call/util.c
    pebble/services/phone_pp/service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_phone_call
  SOURCES
    pebble/services/phone_call/service.c
)

pbl_clar_test(test_debounced_connection_service
  SOURCES
    tests/fakes/fake_rtc.c
    tests/fakes/fake_session.c
    pebble/services/debounced_connection_service/service.c
    pebble/services/clock/time.c
)

pbl_clar_test(test_timeline_item
  MODULES timeline_item time rand
  SOURCES
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_clock
  MODULES resources time
  SOURCES
    subsys/i18n/i18n.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_events.c
    pebble/services/timezone_database/service.c
    pebble/services/clock/service.c
    pebble/applib/pbl_std/strftime.c
    pebble/applib/pbl_std/timelocal.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_timezone_database
  SOURCES
    tests/fakes/fake_spi_flash.c
    pebble/services/timezone_database/service.c
  OVERRIDES test_timezone  DEPENDS pbl_timezone_fixture
)

pbl_clar_test(test_wakeup
  MODULES resources settings rand
  SOURCES
    pebble/process_management/pebble_process_info.c
    pebble/services/wakeup/service.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_migrate_wakeup
  MODULES resources settings rand
  SOURCES
    pebble/process_management/pebble_process_info.c
    pebble/services/wakeup/service.c
    tests/fakes/fake_bootbits.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_registry_endpoint
  SOURCES
    pebble/services/registry_endpoint/service.c
)

pbl_clar_test(test_audio_endpoint
  SOURCES
    pebble/services/audio_endpoint/service.c
    tests/fakes/fake_session.c
)

pbl_clar_test(test_speaker_service
  SOURCES
    pebble/services/speaker/speaker_service.c
    pebble/services/speaker/note_sequence.c
    pebble/services/speaker/pcm_stream.c
    pebble/services/speaker/track_player.c
    tests/fakes/fake_events.c
  DEFINES CONFIG_SPEAKER=1 CONFIG_SERVICE_SPEAKER_STANDBY_MS=0 CONFIG_BATTERY_HIGH_LOAD=1
  OVERRIDES dummy_board
)

pbl_clar_test(test_transcription
  SOURCES
    pebble/services/voice/transcription.c
)

pbl_clar_test(test_voice_endpoint
  MODULES rand
  SOURCES
    pebble/services/voice/transcription.c
    pebble/services/voice_endpoint/service.c
    tests/fakes/fake_session.c
  DEFINES CONFIG_MIC
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_cache
  MODULES settings
  SOURCES
    pebble/services/process_management/app_file.c
    pebble/services/app_cache/service.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_mic_manager
  SOURCES
    pebble/services/mic_manager/mic_manager.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_mic_stream
  SOURCES
    pebble/services/mic_stream/mic_stream.c
    pebble/services/mic_manager/mic_manager.c
    tests/fakes/fake_events.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_install_manager
  MODULES settings rand gtypes
  SOURCES
    pebble/resource/resource.c
    pebble/resource/resource_storage.c
    pebble/resource/resource_storage_builtin.c
    pebble/resource/resource_storage_file.c
    pebble/resource/resource_storage_flash.c
    pebble/services/process_management/app_file.c
    pebble/services/app_cache/service.c
    pebble/services/process_management/app_storage.c
    pebble/process_management/app_install_manager.c
    pebble/process_management/pebble_process_md.c
    pebble/services/blob_db/app_db.c
    pebble/process_management/pebble_process_info.c
    drivers/flash/flash_crc.c
    tests/fakes/fake_rtc.c
    tests/fixtures/resources/builtin_resources.auto.c
    tests/fakes/fake_system_app_registry_apps.c
  OVERRIDES dummy_board fake_app_registry
)

pbl_clar_test(test_app_menu_data_source
  MODULES settings rand gtypes
  SOURCES
    drivers/flash/flash_crc.c
    pebble/process_management/app_install_manager.c
    pebble/process_management/app_menu_data_source.c
    pebble/process_management/pebble_process_info.c
    pebble/process_management/pebble_process_md.c
    pebble/resource/resource.c
    pebble/resource/resource_storage.c
    pebble/resource/resource_storage_builtin.c
    pebble/resource/resource_storage_file.c
    pebble/resource/resource_storage_flash.c
    pebble/services/app_cache/service.c
    pebble/services/blob_db/app_db.c
    pebble/services/process_management/app_file.c
    pebble/services/process_management/app_order_storage.c
    pebble/services/process_management/app_storage.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_system_app_registry_apps.c
    tests/fixtures/resources/builtin_resources.auto.c
  OVERRIDES dummy_board fake_app_registry
)

pbl_clar_test(test_process_loader_storage
  SOURCES
    lib/util/build_id.c
    pebble/kernel/util/segment.c
    pebble/process_management/pebble_process_info.c
    pebble/process_management/pebble_process_md.c
    pebble/services/process_management/app_storage.c
    pebble/services/process_management/process_loader_storage.c
    pebble/process_management/process_runtime.c
    subsys/loader/loader.c
  DEFINES
    CONFIG_LOADER_LOG_LEVEL=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_fetch_endpoint
  MODULES rand
  SOURCES
    pebble/services/app_fetch_endpoint/service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_session.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_manager
  SOURCES
    drivers/base/device.c
    drivers/timer/timer.c
    drivers/imu/accel.c
    pebble/services/accel_manager/service.c
    tests/fakes/fake_events.c
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_SERVICE_ACCEL_MANAGER_BATCH_SAMPLES=25
    CONFIG_DEVICE_LOG_LEVEL=0
    PBL_NO_LINKER_SCRIPT
  OVERRIDES dummy_board
)

pbl_clar_test(test_shared_prf_storage_v3
  SOURCES
    pebble/services/shared_prf_storage/shared_prf_storage.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS emery
)

pbl_clar_test(test_app_file
  SOURCES
    pebble/services/process_management/app_file.c
)

pbl_clar_test(test_do_not_disturb
  MODULES settings time
  SOURCES
    pebble/services/notifications/alerts_preferences.c
    pebble/services/notifications/do_not_disturb.c
    subsys/cron/cron.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_weather_service
  MODULES settings time
  SOURCES
    tests/fakes/fake_rtc.c
    pebble/services/blob_db/weather_db.c
    pebble/services/weather/weather_service.c
    tests/pebble/services/blob_db/weather_data_shared.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_vibe
  SOURCES
    pebble/services/vibe_pattern/service.c
    pebble/applib/ui/vibes.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_vibe_intensity
  SOURCES
    pebble/services/vibes/vibe_intensity.c
)

pbl_clar_test(test_vibe_score
  SOURCES
    pebble/services/vibes/vibe_score.c
)

pbl_clar_test(test_vibe_score_info
  SOURCES
    pebble/services/vibes/vibe_score_info.c
)

pbl_clar_test(test_touch
  SOURCES
    pebble/services/touch/touch.c
    tests/fakes/fake_events.c
)

pbl_clar_test(test_touch_session
  SOURCES
    pebble/services/touch/touch_session.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_hrm_manager
  SOURCES
    pebble/services/hrm/hrm_manager.c
    kernel/tick.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_accel_service.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_app_glance_service
  SOURCES
    pebble/services/app_glances/app_glance_service.c
    pebble/services/blob_db/app_glance_db.c
    pebble/services/timeline/attribute.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_settings.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_clock_time
  MODULES time
  SOURCES
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_mic_capture
  SOURCES
    pebble/services/mic_capture/mic_capture.c
    pebble/services/mic_manager/mic_manager.c
    pebble/services/blob_db/app_db.c
    tests/fakes/fake_events.c
    tests/fakes/fake_settings.c
  DEFINES CONFIG_SERVICE_MIC_CAPTURE
  OVERRIDES dummy_board
)
