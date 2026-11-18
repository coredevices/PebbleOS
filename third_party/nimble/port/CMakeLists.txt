# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  nimble_port_pebble.c
  npl_os_pebble.c
)
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_NRF52 nrf52.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_TRNG trng.c)
