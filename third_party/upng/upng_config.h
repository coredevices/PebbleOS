/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/applib_malloc.auto.h>

#define UPNG_MALLOC(size) applib_malloc(size)
#define UPNG_FREE(ptr)    applib_free(ptr)
