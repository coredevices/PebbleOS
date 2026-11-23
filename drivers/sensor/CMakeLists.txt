# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(sensor.c)
pbl_library_sources_ifdef(CONFIG_SENSOR_SHELL shell.c)
pbl_library_sources_ifdef(CONFIG_ACCEL_LIS2DW12 lis2dw12/lis2dw12.c)
pbl_library_sources_ifdef(CONFIG_ACCEL_LSM6DSO lsm6dso/lsm6dso.c)
pbl_library_sources_ifdef(CONFIG_ACCEL_SIM accel_sim.c)
pbl_library_sources_ifdef(CONFIG_MAG_MMC5603NJ mmc5603nj/mmc5603nj.c)
pbl_library_sources_ifdef(CONFIG_MAG_SIM mag_sim.c)
pbl_library_sources_ifdef(CONFIG_PRESSURE_BMP390 bmp390.c)
pbl_library_sources_ifdef(CONFIG_TEMP_NRF5 temp_nrf5.c)
pbl_library_sources_ifdef(CONFIG_TEMP_SF32LB temp_sf32lb.c)
pbl_library_sources_ifdef(CONFIG_TEMP_SIM temp_sim.c)
