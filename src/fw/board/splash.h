/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if BOARD_OBELIX_EVT || BOARD_OBELIX_DVT || BOARD_OBELIX_PVT || BOARD_OBELIX_BB || BOARD_OBELIX_BB2
#include "splash/splash_obelix.xbm"
#else
#error "Unknown splash definition for board"
#endif // BOARD_*
