/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler/gcc.h"

#undef PBL_OPTIMIZE_IMPL
#define PBL_OPTIMIZE_IMPL(level)

#undef PBL_EXTERNALLY_VISIBLE_IMPL
#define PBL_EXTERNALLY_VISIBLE_IMPL
