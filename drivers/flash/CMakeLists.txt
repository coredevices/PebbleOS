# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(flash.c)
pbl_library_sources_ifdef(CONFIG_FLASH_CRC flash_crc.c)
pbl_library_sources_ifdef(CONFIG_FLASH_OTP otp.c)
pbl_library_sources_ifdef(CONFIG_FLASH_SPI_NOR spi_nor.c)
pbl_library_sources_ifdef(CONFIG_FLASH_SPI_NOR_GD25LQ255E gd25lq255e.c)
pbl_library_sources_ifdef(CONFIG_FLASH_SPI_NOR_GD25Q256E gd25q256e.c)
pbl_library_sources_ifdef(CONFIG_FLASH_SPI_NOR_PY25Q128HA py25q128ha.c)
pbl_library_sources_ifdef(CONFIG_FLASH_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_FLASH_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_FLASH_SHELL shell.c)
