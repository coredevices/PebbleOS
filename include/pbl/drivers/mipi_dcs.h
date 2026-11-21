/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_mipi_dcs MIPI DCS
 * @ingroup drivers
 * @brief MIPI DCS commands, which panels take over MIPI DBI and MIPI DSI.
 * @{
 */

#define PBL_MIPI_DCS_SOFT_RESET             0x01U
#define PBL_MIPI_DCS_GET_DISPLAY_ID         0x04U
#define PBL_MIPI_DCS_ENTER_SLEEP_MODE       0x10U
#define PBL_MIPI_DCS_EXIT_SLEEP_MODE        0x11U
#define PBL_MIPI_DCS_SET_DISPLAY_OFF        0x28U
#define PBL_MIPI_DCS_SET_DISPLAY_ON         0x29U
#define PBL_MIPI_DCS_SET_COLUMN_ADDRESS     0x2AU
#define PBL_MIPI_DCS_SET_PAGE_ADDRESS       0x2BU
#define PBL_MIPI_DCS_WRITE_MEMORY_START     0x2CU
#define PBL_MIPI_DCS_SET_TEAR_OFF           0x34U
#define PBL_MIPI_DCS_SET_TEAR_ON            0x35U
#define PBL_MIPI_DCS_SET_ADDRESS_MODE       0x36U
#define PBL_MIPI_DCS_SET_PIXEL_FORMAT       0x3AU
#define PBL_MIPI_DCS_SET_TEAR_SCANLINE      0x44U
#define PBL_MIPI_DCS_SET_DISPLAY_BRIGHTNESS 0x51U
#define PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY  0x53U

/** @} */
