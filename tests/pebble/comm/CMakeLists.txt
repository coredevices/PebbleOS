# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ams
  SOURCES
    pebble/comm/ble/kernel_le_client/ams/ams_util.c
    pebble/comm/ble/kernel_le_client/ams/ams.c
    pebble/services/music/service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ams_util
  SOURCES
    pebble/comm/ble/kernel_le_client/ams/ams_util.c
)

pbl_clar_test(test_gap_le_advert
  SOURCES
    pebble/comm/ble/gap_le_advert.c
    tests/fakes/fake_bt_advert.c
    tests/fakes/fake_rtc.c
    pebble/services/clock/time.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_bt_conn_mgr
  SOURCES
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_rtc.c
    pebble/services/clock/time.c
)

pbl_clar_test(test_gatt_client_accessors
  MODULES rand
  SOURCES
    pebble/comm/ble/gap_le_connection.c
    pebble/comm/ble/gatt_client_accessors.c
    pebble/comm/ble/gatt_client_discovery.c
    pebble/comm/ble/gatt_service_changed.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_gap_le_connect_params.c
)

pbl_clar_test(test_gatt_client_discovery
  MODULES rand
  SOURCES
    pebble/comm/ble/gap_le_connection.c
    pebble/comm/ble/gatt_client_accessors.c
    pebble/comm/ble/gatt_client_discovery.c
    pebble/comm/ble/gatt_service_changed.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_bt_gatt.c
)

pbl_clar_test(test_gatt_client_subscriptions
  MODULES rand
  SOURCES
    pebble/comm/ble/gap_le_connection.c
    pebble/comm/ble/gatt_client_accessors.c
    pebble/comm/ble/gatt_client_discovery.c
    pebble/comm/ble/gatt_client_subscriptions.c
    pebble/comm/ble/gatt_service_changed.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_events.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_BLE_GATT_NOTIF_WRITE_TIMEOUT_MS=100 CONFIG_BLE_GATT_SUBSCRIPTION_DEPTH=4
)

pbl_clar_test(test_gatt_service_changed_client
  MODULES rand
  SOURCES
    pebble/comm/ble/gatt_client_discovery.c
    pebble/comm/ble/gatt_service_changed.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_gatt_service_changed_server
  MODULES rand
  SOURCES
    pebble/comm/ble/gap_le_connection.c
    pebble/comm/ble/gatt_client_discovery.c
    pebble/comm/ble/gatt_service_changed.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs_util
  MODULES time
  SOURCES
    pebble/comm/ble/kernel_le_client/ancs/ancs_util.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs
  MODULES timeline_item time rand
  SOURCES
    pebble/comm/ble/kernel_le_client/ancs/ancs_util.c
    pebble/comm/ble/kernel_le_client/ancs/ancs.c
    pebble/comm/ble/kernel_le_client/ancs/ancs_app_name_storage.c
    tests/fakes/fake_events.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_rtc.c
    pebble/services/notifications/ancs/ancs_notifications.c
    pebble/services/notifications/ancs/ancs_notifications_util.c
    pebble/services/notifications/ancs/ancs_item.c
    pebble/services/notifications/ancs/ancs_filtering.c
    pebble/services/notifications/ancs/ancs_phone_call.c
)

pbl_clar_test(test_kernel_le_client
  MODULES rand
  SOURCES
    pebble/comm/ble/kernel_le_client/kernel_le_client.c
)

# Skipped for now; see pbl_test_broken() in tests/CMakeLists.txt.
pbl_clar_test(test_gap_le_connect
  SOURCES
    pebble/comm/ble/gap_le_connect.c
    pebble/comm/ble/gap_le_connection.c
    pebble/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_events.c
    tests/fakes/fake_GAPAPI.c
    tests/fakes/fake_bluetooth_persistent_storage.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_HCIAPI.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_qemu_serial
  SOURCES
    pebble/comm/qemu/serial_util.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)
