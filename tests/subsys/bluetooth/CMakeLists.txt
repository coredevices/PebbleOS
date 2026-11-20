# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ancs_client
  SOURCES
    subsys/bluetooth/clients/ancs/ancs.c
    tests/fakes/fake_gattc_service.c
  INCLUDES third_party/nimble/mynewt-nimble/nimble/host/include
)
