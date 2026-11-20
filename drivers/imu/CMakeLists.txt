# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ACCEL)
  pbl_library_named(driver_accel)
  pbl_library_sources(accel.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_SIM accel_sim.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_LIS2DW12 lis2dw12/lis2dw12.c)
  pbl_library_sources_ifdef(CONFIG_ACCEL_LSM6DSO lsm6dso/lsm6dso.c)
  pbl_library_sources_ifdef(CONFIG_IMU_SHELL shell.c)
endif()
