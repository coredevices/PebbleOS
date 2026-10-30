# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_flash_api
  SOURCES
    drivers/base/device.c
    drivers/flash/flash.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_FLASH_LOG_LEVEL=0
    PBL_NO_LINKER_SCRIPT
  OVERRIDES dummy_board
)

pbl_clar_test(test_spi_nor
  SOURCES
    drivers/base/device.c
    drivers/flash/flash.c
    drivers/flash/gd25lq255e.c
    drivers/flash/gd25q256e.c
    drivers/flash/otp.c
    drivers/flash/spi_nor.c
    drivers/otp/otp_flash.c
    drivers/spi_mem/spi_mem.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_FLASH_LOG_LEVEL=0
    CONFIG_DRIVER_SPI_MEM_LOG_LEVEL=0
    PBL_NO_LINKER_SCRIPT
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_qemu
  SOURCES
    drivers/base/device.c
    drivers/imu/accel.c
    drivers/imu/qemu.c
    tests/fakes/fake_rtc.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    PBL_NO_LINKER_SCRIPT
  OVERRIDES dummy_board
)
