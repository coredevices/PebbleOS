/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl_arch_posix.h>

#define arch_cpu_idle() pbl_posix_cpu_wait(UINT64_MAX)
