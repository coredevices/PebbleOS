# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_session
  SOURCES
    pebble/kernel/pp.c
    subsys/pp/session.c
    subsys/pp/send_queue.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_receive
  SOURCES
    subsys/pp/session.c
    subsys/pp/send_queue.c
    subsys/pp/receive.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_send_queue
  SOURCES
    subsys/pp/send_queue.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_send_buffer
  SOURCES
    subsys/pp/send_buffer.c
    subsys/pp/send_queue.c
    tests/fakes/fake_rtc.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_version
  SOURCES
    subsys/pp/version.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_kernel_receiver
  SOURCES
    subsys/pp/kernel_receiver.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_meta
  SOURCES
    subsys/pp/meta.c
    tests/fakes/fake_session.c
  INCLUDES subsys/pp
  DEFINES
    CONFIG_PP_LOG_LEVEL=0
)

pbl_clar_test(test_ppogatt
  MODULES rand
  SOURCES
    pebble/services/clock/time.c
    subsys/logging/hexdump.c
    subsys/pp/transports/gatt/ppogatt.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_session.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  DEFINES
    CONFIG_NIMBLE_LOG_LEVEL=0
)

pbl_clar_test(test_ppogatt_reversed
  MODULES rand
  SOURCES
    pebble/services/clock/time.c
    subsys/logging/hexdump.c
    subsys/pp/transports/gatt/ppogatt.c
    tests/fakes/fake_gatt_client_operations.c
    tests/fakes/fake_gatt_client_subscriptions.c
    tests/fakes/fake_session.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  DEFINES
    CONFIG_NIMBLE_LOG_LEVEL=0
)
