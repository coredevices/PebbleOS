# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ams
  SOURCES
    pebble/comm/ble/kernel_le_client/ams/ams_util.c
    pebble/comm/ble/kernel_le_client/ams/ams.c
    subsys/bluetooth/clients/ams/ams.c
    pebble/services/music/service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_gattc_service.c
    tests/fakes/fake_rtc.c
  INCLUDES third_party/nimble/mynewt-nimble/nimble/host/include
  DEFINES CONFIG_MUSIC_ALBUM_ART_RECENTS=0
)

pbl_clar_test(test_ams_util
  SOURCES
    pebble/comm/ble/kernel_le_client/ams/ams_util.c
)

pbl_clar_test(test_ancs
  MODULES timeline_item time rand
  SOURCES
    pebble/comm/ble/kernel_le_client/ancs/ancs.c
    pebble/comm/ble/kernel_le_client/ancs/ancs_app_name_storage.c
    subsys/bluetooth/clients/ancs/ancs.c
    tests/fakes/fake_events.c
    tests/fakes/fake_gattc_service.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_rtc.c
    pebble/services/notifications/ancs/ancs_notifications.c
    pebble/services/notifications/ancs/ancs_notifications_util.c
    pebble/services/notifications/ancs/ancs_item.c
    pebble/services/notifications/ancs/ancs_filtering.c
    pebble/services/notifications/ancs/ancs_phone_call.c
  INCLUDES third_party/nimble/mynewt-nimble/nimble/host/include
)

pbl_clar_test(test_qemu_serial
  SOURCES
    pebble/comm/qemu/serial_util.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)
