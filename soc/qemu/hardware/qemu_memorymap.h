/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! QEMU Pebble machine peripheral base addresses.

#pragma once

#define QEMU_TIMER0_BASE  0x40003000
#define QEMU_TIMER1_BASE  0x40004000
#define QEMU_RTC_BASE     0x40005000
#define QEMU_SYSCTRL_BASE 0x40007000
#define QEMU_WDOG_BASE    0x40013000

#define QEMU_EXTFLASH_XIP_BASE 0x10000000
