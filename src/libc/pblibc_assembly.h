/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if __ARM_ARCH_ISA_THUMB >= 2
# define MEMCPY_IMPLEMENTED_IN_ASSEMBLY 1
# define MEMSET_IMPLEMENTED_IN_ASSEMBLY 1
#else
# define MEMCPY_IMPLEMENTED_IN_ASSEMBLY 0
# define MEMSET_IMPLEMENTED_IN_ASSEMBLY 0
#endif
