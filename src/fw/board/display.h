/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @internal
//! data type that's used to store row data infos in a space-efficient manner
typedef struct {
  uint16_t offset;
  uint8_t min_x;
  uint8_t max_x;
} GBitmapDataRowInfoInternal;

// FIXME: PBL-21055 Fix SDK exporter failing to crawl framebuffer headers
#if !defined(SDK)

// FIXME: PBL-21049 Fix platform abstraction and board definition scheme
#ifdef UNITTEST
// Do nothing, a unit-test's wscript specifies platforms=[]
// used by waftools/pebble_test.py to define these includes per test
#else

#if BOARD_BIGBOARD
#include "displays/display_tintin.h"
#elif BOARD_EV2_4
#include "displays/display_tintin.h"
#elif BOARD_BB2
#include "displays/display_tintin.h"
#elif BOARD_V1_5
#include "displays/display_tintin.h"
#elif BOARD_V2_0
#include "displays/display_tintin.h"
#elif BOARD_SNOWY_BB
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_EVT
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_EVT2
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_BB2
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_DVT
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_S3
#include "displays/display_snowy.h"
#elif BOARD_SNOWY_EMERY
#include "displays/display_robert.h"
#elif BOARD_SPALDING_BB2
#include "displays/display_spalding.h"
#elif BOARD_SPALDING_EVT
#include "displays/display_spalding.h"
#elif BOARD_SPALDING
#include "displays/display_spalding.h"
#elif BOARD_SILK_EVT
#include "displays/display_silk.h"
#elif BOARD_SILK_BB
#include "displays/display_silk.h"
#elif BOARD_SILK_BB2
#include "displays/display_silk.h"
#elif BOARD_SILK
#include "displays/display_silk.h"
#elif BOARD_SILK_FLINT
#include "displays/display_silk.h"
#elif BOARD_CALVIN_BB
#include "displays/display_silk.h"
#elif BOARD_ASTERIX
#include "displays/display_silk.h"
#elif BOARD_OBELIX_EVT || BOARD_OBELIX_DVT || BOARD_OBELIX_PVT || BOARD_OBELIX_BB || BOARD_OBELIX_BB2
#include "displays/display_obelix.h"
#elif BOARD_CUTTS_BB
#include "displays/display_snowy.h"
#elif BOARD_ROBERT_BB
#include "displays/display_robert.h"
#elif BOARD_ROBERT_BB2
#include "displays/display_robert.h"
#elif BOARD_ROBERT_EVT
#include "displays/display_robert_evt.h"
#else
#error "Unknown display definition for board"
#endif // BOARD_*

#endif // UNITTEST

// For backwards compatibility, new code should use PBL_DISPLAY_WIDTH and PBL_DISPLAY_HEIGHT
#if !defined(DISP_COLS) || !defined(DISP_ROWS)
#define DISP_COLS PBL_DISPLAY_WIDTH
#define DISP_ROWS PBL_DISPLAY_HEIGHT
#endif // DISP_COLS || DISP_ROWS

#endif // !SDK
