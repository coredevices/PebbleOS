/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef CONFIG_BOARD_ASTERIX
#include <board/boards/board_asterix.h>
#elif defined(CONFIG_BOARD_OBELIX_DVT) || defined(CONFIG_BOARD_OBELIX_PVT) || \
    defined(CONFIG_BOARD_OBELIX_BB2)
#include <board/boards/board_obelix.h>
#elif defined(CONFIG_BOARD_GETAFIX_DVT) || defined(CONFIG_BOARD_GETAFIX_DVT2)
#include <board/boards/board_getafix.h>
#elifdef CONFIG_BOARD_QEMU_EMERY
#include <board/boards/board_qemu_emery.h>
#elifdef CONFIG_SOC_POSIX
#include <board/boards/board_native.h>
#elifdef CONFIG_BOARD_QEMU_FLINT
#include <board/boards/board_qemu_flint.h>
#elifdef CONFIG_BOARD_QEMU_GABBRO
#include <board/boards/board_qemu_gabbro.h>
#else
#error "Unknown board definition"
#endif
