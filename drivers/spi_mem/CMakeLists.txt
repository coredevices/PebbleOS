# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(spi_mem.c)
pbl_library_sources_ifdef(CONFIG_SPI_MEM_NRF5_QSPI nrf5_qspi.c)
pbl_library_sources_ifdef(CONFIG_SPI_MEM_SF32LB52_MPI sf32lb52_mpi.c)
