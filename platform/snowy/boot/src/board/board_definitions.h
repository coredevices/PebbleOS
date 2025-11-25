/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if BOARD_SNOWY_BB
#include "board_snowy_bb.h" // prototypes for Snowy bigboard
#elif BOARD_SNOWY_EVT
#include "board_snowy_evt.h" // prototypes for Snowy EVT
#elif BOARD_SNOWY_EVT2
#include "board_snowy_evt2.h" // prototypes for Snowy EVT2
#elif BOARD_SPALDING
#include "board_snowy_evt2.h" // Close enough
#else
#error "Unknown board definition"
#endif
