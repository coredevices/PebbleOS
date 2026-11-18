# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_FAKE hci_fake.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_SF32LB hci_sf32lb.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_UART hci_uart.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_ID_ADDR_SF32LB52_EFUSE id_addr_sf32lb52.c)

if(CONFIG_NIMBLE_HCI_SF32LB_TRACE_LOG)
  pbl_library_compile_definitions(NIMBLE_HCI_SF32LB_TRACE NIMBLE_HCI_SF32LB_TRACE_LOG)
endif()
