# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ACCEL)
  pbl_library_named(driver_accel)
  pbl_library_sources(accel.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_QEMU qemu.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_POSIX posix.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_LIS2DW12 lis2dw12/lis2dw12.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_LSM6DSO lsm6dso/lsm6dso.c)
  pbl_library_sources_ifdef(CONFIG_IMU_SHELL shell.c)
endif()
if(CONFIG_MAG)
  pbl_library_named(driver_mag)
  pbl_library_sources(mag.c)
  pbl_library_sources_ifdef(CONFIG_MAG_MMC5603NJ mmc5603nj/mmc5603nj.c)
endif()
