/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup mcu_interrupts Execution state
 * @ingroup mcu
 * @brief Exception and interrupt-mask state of the CPU.
 *
 * On host builds (unit tests) code never runs in an exception handler.
 * @{
 */

/**
 * @brief Check whether the CPU is executing an exception handler.
 *
 * @return true in an ISR or fault handler.
 */
inline static bool mcu_state_is_isr(void);

/**
 * @brief Get the priority of the exception handler being executed.
 *
 * Lower numbers mean higher priority. Handlers more urgent than @ref PBL_IRQ_PRIO_MAX_SYSCALL
 * must not call the kernel.
 *
 * @return NVIC priority in controller units, or ~0 outside an exception handler.
 */
inline static uint32_t mcu_state_get_isr_priority(void);

/**
 * @brief Check whether interrupts are globally enabled.
 *
 * @return false if PRIMASK masks every configurable-priority interrupt.
 */
bool mcu_state_are_interrupts_enabled(void);

/** @} */

#ifdef __arm__
#include <pbl/mcu/interrupts_arm.inl.h>
#else
#include <pbl/mcu/interrupts_stubs.inl.h>
#endif
