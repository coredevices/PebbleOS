/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// FIXME: PBL-21049 Fix platform abstraction and board definition scheme
#if BOARD_SNOWY_BB2
#include "boards/board_snowy.h" // prototypes for Snowy BB2 are identical to EVT2
#elif BOARD_SNOWY_DVT
#include "boards/board_snowy.h" // prototypes for DVT, electrically identical to EVT2
#elif BOARD_SPALDING_BB2
#include "boards/board_snowy.h" // prototypes for Spalding BB2, Snowy BB2s with a Spalding display
#elif BOARD_SPALDING
#include "boards/board_spalding_evt.h" // prototypes for Spalding MP
#elif BOARD_SNOWY_EMERY
#include "boards/board_snowy.h" // Snowy hardware with Robert display and resources
#elif BOARD_SPALDING_GABBRO
#include "boards/board_spalding_evt.h" // Spalding hardware with Getafix display and resources
#elif BOARD_SILK_BB2
#include "boards/board_silk.h"
#elif BOARD_SILK
#include "boards/board_silk.h"
#elif BOARD_SILK_FLINT
#include "boards/board_silk.h"
#elif BOARD_ASTERIX
#include "boards/board_asterix.h"
#elif BOARD_OBELIX_DVT || BOARD_OBELIX_PVT || BOARD_OBELIX_BB2
#include "boards/board_obelix.h"
#elif BOARD_GETAFIX_EVT || BOARD_GETAFIX_DVT
#include "boards/board_getafix.h"
#else
#error "Unknown board definition"
#endif
