/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

//! static in firmware, weak in unit tests so private functions can be overridden.
#if UNITTEST
#define PBL_T_STATIC PBL_WEAK
#else
#define PBL_T_STATIC static
#endif

//! Nothing in firmware, weak in unit tests so global functions can be mocked.
#if UNITTEST
#define PBL_T_MOCKABLE PBL_WEAK
#else
#define PBL_T_MOCKABLE
#endif
