/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if BOARD_ROBERT_BB || BOARD_ROBERT_BB2
#include "board_robert_bb.h"
#elif BOARD_ROBERT_EVT
#include "board_robert_evt.h"
#else
#error "Unknown board definition"
#endif
