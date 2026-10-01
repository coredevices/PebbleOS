# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ams
  SOURCES
    fw/comm/ble/kernel_le_client/ams/ams_util.c
    fw/comm/ble/kernel_le_client/ams/ams.c
    fw/services/music/service.c
    tests/fakes/fake_events.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ams_util
  SOURCES
    fw/comm/ble/kernel_le_client/ams/ams_util.c
)

pbl_clar_test(test_gap_le_advert
  SOURCES
    fw/comm/ble/gap_le_advert.c
    fw/services/regular_timer/service.c
    tests/fakes/fake_bt_advert.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_bt_conn_mgr
  SOURCES
    fw/comm/internals/bt_conn_mgr.c
    fw/services/regular_timer/service.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_gatt_client_accessors
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/comm/ble/gap_le_connection.c
    fw/comm/ble/gatt_client_accessors.c
    fw/comm/ble/gatt_client_discovery.c
    fw/comm/ble/gatt_service_changed.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_gap_le_connect_params.c
)

pbl_clar_test(test_gatt_client_discovery
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/comm/ble/gap_le_connection.c
    fw/comm/ble/gatt_client_accessors.c
    fw/comm/ble/gatt_client_discovery.c
    fw/comm/ble/gatt_service_changed.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_bt_gatt.c
)

pbl_clar_test(test_gatt_client_subscriptions
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/comm/ble/gap_le_connection.c
    fw/comm/ble/gap_le_task.c
    fw/comm/ble/gatt_client_accessors.c
    fw/comm/ble/gatt_client_discovery.c
    fw/comm/ble/gatt_client_subscriptions.c
    fw/comm/ble/gatt_service_changed.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_events.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
  DEFINES CONFIG_BLE_GATT_NOTIF_WRITE_TIMEOUT_MS=100 CONFIG_BLE_GATT_SUBSCRIPTION_DEPTH=4
)

pbl_clar_test(test_gatt_service_changed_client
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/comm/ble/gatt_client_discovery.c
    fw/comm/ble/gatt_service_changed.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_gatt_service_changed_server
  SOURCES
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/comm/ble/gap_le_connection.c
    fw/comm/ble/gatt_client_discovery.c
    fw/comm/ble/gatt_service_changed.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_bt_gatt.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs_util
  SOURCES
    lib/c/time/mktime.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    fw/comm/ble/kernel_le_client/ancs/ancs_util.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ancs
  SOURCES
    lib/c/time/mktime.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    fw/comm/ble/kernel_le_client/ancs/ancs_util.c
    fw/comm/ble/kernel_le_client/ancs/ancs.c
    fw/comm/ble/kernel_le_client/ancs/ancs_app_name_storage.c
    fw/services/regular_timer/service.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_events.c
    tests/fakes/fake_kernel_services_notifications.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_rtc.c
    fw/services/notifications/ancs/ancs_notifications.c
    fw/services/notifications/ancs/ancs_notifications_util.c
    fw/services/notifications/ancs/ancs_item.c
    fw/services/notifications/ancs/ancs_filtering.c
    fw/services/notifications/ancs/ancs_phone_call.c
    fw/services/timeline/item.c
    fw/services/timeline/attributes_actions.c
    fw/services/timeline/attribute_group.c
    fw/services/timeline/attribute.c
)

pbl_clar_test(test_kernel_le_client
  SOURCES
    fw/comm/ble/kernel_le_client/kernel_le_client.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)

pbl_clar_test(test_ppogatt_v0
  SOURCE test_ppogatt.c
  SOURCES
    fw/system/hexdump.c
    fw/comm/ble/kernel_le_client/ppogatt/ppogatt.c
    fw/services/regular_timer/service.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_session.c
  DEFINES USE_PPOGATT_VERSION=0
)

pbl_clar_test(test_ppogatt_v1
  SOURCE test_ppogatt.c
  SOURCES
    fw/system/hexdump.c
    fw/comm/ble/kernel_le_client/ppogatt/ppogatt.c
    fw/services/regular_timer/service.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_session.c
  DEFINES USE_PPOGATT_VERSION=1
)

# Skipped for now; see pbl_test_broken() in tests/CMakeLists.txt.
pbl_clar_test(test_gap_le_connect
  SOURCES
    fw/comm/ble/gap_le_connect.c
    fw/comm/ble/gap_le_connection.c
    fw/comm/ble/gap_le_task.c
    fw/comm/internals/bt_conn_mgr.c
    tests/fakes/fake_events.c
    tests/fakes/fake_GAPAPI.c
    tests/fakes/fake_bluetooth_persistent_storage.c
    tests/fakes/fake_gap_le_connect_params.c
    tests/fakes/fake_HCIAPI.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)
