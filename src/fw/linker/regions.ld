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
__unpriv_ro_bss_size__ = 2K;
__stack_guard_size__ = 32;
