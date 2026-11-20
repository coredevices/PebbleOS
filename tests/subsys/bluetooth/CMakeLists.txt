# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_gattc_service
  SOURCES
    subsys/bluetooth/clients/gatt.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  DEFINES CONFIG_NIMBLE_LOG_LEVEL=0
)

pbl_clar_test(test_ancs_client
  SOURCES
    subsys/bluetooth/clients/ancs/ancs.c
    tests/fakes/fake_gattc_service.c
  INCLUDES third_party/nimble/mynewt-nimble/nimble/host/include
)

pbl_clar_test(test_ams_client
  SOURCES
    subsys/bluetooth/clients/ams/ams.c
    tests/fakes/fake_gattc_service.c
  INCLUDES third_party/nimble/mynewt-nimble/nimble/host/include
)
