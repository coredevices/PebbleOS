/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! QEMU Pebble machine peripheral base addresses.

#pragma once

#define QEMU_UART0_BASE      0x40000000
#define QEMU_UART1_BASE      0x40001000
#define QEMU_UART2_BASE      0x40002000
#define QEMU_TIMER0_BASE     0x40003000
#define QEMU_TIMER1_BASE     0x40004000
#define QEMU_RTC_BASE        0x40005000
#define QEMU_GPIO_BASE       0x40006000
#define QEMU_SYSCTRL_BASE    0x40007000
#define QEMU_DISPLAY_BASE    0x40008000
#define QEMU_DISPLAY_FB_BASE 0x50000000
#define QEMU_EXTFLASH_BASE   0x40010000
#define QEMU_TOUCH_BASE      0x40011000
#define QEMU_AUDIO_BASE      0x40012000
#define QEMU_WDOG_BASE       0x40013000
#define QEMU_UART3_BASE      0x40014000

#define QEMU_EXTFLASH_XIP_BASE 0x10000000
