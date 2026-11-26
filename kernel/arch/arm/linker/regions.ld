/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * MPU region sizing.
 */

/*
Our MPU only supports powers of two for region sizes. This means we have to upfront define
how big our privileged regions are, since there's no way of doing "Align to power of two"
in a linker script.

If a section is too small, you'll see an error like this:
cannot move location counter backwards (from 0000000008011754 to 0000000008010800)
If you see this, you need to bump the size to the next power of two.
*/
#ifdef CONFIG_USERSPACE
__unpriv_ro_bss_size__ = 2K;
#else
__unpriv_ro_bss_size__ = 0;
#endif

/*
The size to pad a section to so a single MPU region covers it exactly, and its start alignment:
ARMv8-M regions have a 32-byte granularity; an ARMv7-M region is a power-of-two block aligned to
its size, of which it can leave out whole eighths at either end.
*/
#ifdef CONFIG_ARMV8_M_MAINLINE
#define PBL_MPU_REGION_SIZE(size)  ALIGN(MAX(32, (size)), 32)
#define PBL_MPU_REGION_ALIGN(size) 32
#else
#define PBL_MPU_REGION_ALIGN(size) (1 << LOG2CEIL(MAX(32, (size))))
#define PBL_MPU_REGION_SIZE(size)  ALIGN(MAX(32, (size)), PBL_MPU_REGION_ALIGN(size) / 8)
#endif
