# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_FAKE hci_fake.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_SF32LB hci_sf32lb.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_HCI_UART hci_uart.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_ID_ADDR_POSIX id_addr_posix.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_ID_ADDR_QEMU id_addr_qemu.c)
pbl_library_sources_ifdef(CONFIG_NIMBLE_ID_ADDR_SF32LB52_EFUSE id_addr_sf32lb52.c)

if(CONFIG_NIMBLE_ID_ADDR_POSIX)
  pbl_host_sources(id_addr_posix_bottom.c)
endif()
