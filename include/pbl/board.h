/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#ifdef CONFIG_BOARD_DISPLAY_CORNERS
//! Pixels hidden at each end of the first rows of the display, mirrored in the last rows, by
//! tinted rounded corners on the glass.
extern const uint8_t BOARD_DISPLAY_CORNERS[];
//! Number of @ref BOARD_DISPLAY_CORNERS entries.
extern const uint8_t BOARD_DISPLAY_NUM_CORNERS;
#endif

#ifdef CONFIG_BOARD_HEADER
#include CONFIG_BOARD_HEADER
#endif
