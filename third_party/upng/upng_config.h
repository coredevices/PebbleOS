/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pbl_malloc.h"

#define UPNG_MALLOC(size) task_malloc(size)
#define UPNG_FREE(ptr)    task_free(ptr)
