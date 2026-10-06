/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Runs the firmware: takes the CPU and calls @p entry, which is expected to
//! start the kernel. Never returns.
void pbl_posix_boot(int (*entry)(void)) __attribute__((noreturn));

//! Runs @p isr as an interrupt. Called from host threads that are not kernel
//! threads; waits until the kernel lets go of the CPU.
void pbl_posix_irq_run(void (*isr)(void *), void *arg);

//! Idle: lets go of the CPU until an interrupt ran or @p timeout_us passed.
void pbl_posix_cpu_wait(uint64_t timeout_us);
