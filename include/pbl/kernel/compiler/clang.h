/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/** @cond INTERNAL_HIDDEN */

#include <pbl/kernel/compiler/gcc.h>

#undef PBL_OPTIMIZE_IMPL
#define PBL_OPTIMIZE_IMPL(level)

#undef PBL_EXTERNALLY_VISIBLE_IMPL
#define PBL_EXTERNALLY_VISIBLE_IMPL

#ifdef __EMSCRIPTEN__
// WebAssembly has no return address to read; Emscripten's emulation walks
// the JavaScript stack, slowly.
#undef PBL_RETURN_ADDRESS_IMPL
#define PBL_RETURN_ADDRESS_IMPL(level) ((void *)0)
#endif

/** @endcond */
