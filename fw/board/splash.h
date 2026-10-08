/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(CONFIG_BOARD_OBELIX_DVT) || defined(CONFIG_BOARD_OBELIX_PVT) || \
    defined(CONFIG_BOARD_OBELIX_BB2)
#include <board/splash/splash_obelix.xbm>
#elif defined(CONFIG_BOARD_GETAFIX_DVT) || defined(CONFIG_BOARD_GETAFIX_DVT2)
#include <board/splash/splash_getafix.xbm>
#elif defined(CONFIG_BOARD_QEMU_EMERY) || defined(CONFIG_BOARD_NATIVE_EMERY)
#include <board/splash/splash_obelix.xbm>
#elif defined(CONFIG_BOARD_QEMU_FLINT)
#include <board/splash/splash_obelix.xbm>
#elif defined(CONFIG_BOARD_QEMU_GABBRO)
#include <board/splash/splash_obelix.xbm>
#else
#error "Unknown splash definition for board"
#endif // BOARD_*
