/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// FIXME: PBL-21049 Fix platform abstraction and board definition scheme
#if BOARD_ASTERIX
#include "boards/board_asterix.h"
#elif BOARD_OBELIX_DVT || BOARD_OBELIX_PVT || BOARD_OBELIX_BB2
#include "boards/board_obelix.h"
#elif BOARD_GETAFIX_EVT || BOARD_GETAFIX_DVT || BOARD_GETAFIX_DVT2
#include "boards/board_getafix.h"
#elif BOARD_QEMU_EMERY
#include "boards/board_qemu_emery.h"
#elif BOARD_QEMU_FLINT
#include "boards/board_qemu_flint.h"
#elif BOARD_QEMU_GABBRO
#include "boards/board_qemu_gabbro.h"
#else
#error "Unknown board definition"
#endif
