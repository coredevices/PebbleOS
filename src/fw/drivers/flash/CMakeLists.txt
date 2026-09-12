# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  flash.c
  flash_crc.c
)
if(CONFIG_FLASH_NRF5_QSPI)
  list(APPEND sources nrf5_qspi.c)
endif()
if(CONFIG_FLASH_SF32LB52_MPI)
  list(APPEND sources sf32lb52_mpi.c)
endif()
if(CONFIG_FLASH_QEMU)
  list(APPEND sources qemu.c)
endif()
if(CONFIG_FLASH_GD25LQ255E)
  list(APPEND sources gd25lq255e.c)
endif()
if(CONFIG_FLASH_GD25Q256E)
  list(APPEND sources gd25q256e.c)
endif()
pbl_library()
pbl_library_sources(${sources})
