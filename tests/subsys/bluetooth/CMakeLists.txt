# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_hci_router
  SOURCES
    subsys/bluetooth/hci_router.c
)

pbl_clar_test(test_lcpu_ring
  SOURCES
    subsys/bluetooth/lcpu_ring.c
)

pbl_clar_test(test_call_audio_dsp
  SOURCES
    subsys/bluetooth/call_audio_dsp.c
    lib/util/math.c
)

pbl_clar_test(test_call_audio_cvsd
  SOURCES
    subsys/bluetooth/call_audio_cvsd.c
  LIBS
    m
)
