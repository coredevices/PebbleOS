/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/types.h>

#include <pbl_arch_idle.h>

/**
 * @defgroup kernel_idle Idle states
 * @ingroup kernel
 * @brief Low-power states the CPU idles in, provided by the SoC.
 *
 * When nothing is runnable, the kernel picks the deepest state that is allowed now and worth
 * entering for the time until the next timeout, programs the system clock event device to wake
 * the CPU up early enough, enters the state, and accounts for the time spent in it from the
 * system clock source. The SoC only puts the CPU to sleep.
 *
 * Idle states need @c CONFIG_CPU_IDLE. Without it the kernel only calls pbl_cpu_idle(), a tick at
 * a time, pbl_idle_state_block() and pbl_idle_state_release() do nothing, and nothing else here is
 * available.
 *
 * @code{.c}
 * const struct pbl_idle_state pbl_soc_idle_states[] = {
 *   {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = pbl_cpu_idle},
 * };
 * const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);
 *
 * int pbl_soc_idle_state_limit(void) {
 *   return 0;
 * }
 * @endcode
 * @{
 */

/** @brief Maximum number of idle states. */
#define PBL_IDLE_STATES_MAX 4

/** @brief Index of an idle state in @ref pbl_soc_idle_states. */
typedef uint8_t pbl_idle_state_id_t;

/** @brief Idle state. */
struct pbl_idle_state {
  /** Name, for statistics. */
  const char *name;
  /** Shortest time until the next timeout worth entering the state for, in ticks. */
  pbl_tick_t min_ticks;
  /** Time the state takes to wake up from, in ticks; the CPU is woken up this early. */
  pbl_tick_t exit_ticks;
  /**
   * Enter the state. Called with every interrupt masked; returns once one is pending, with
   * interrupts still masked.
   */
  void (*enter)(void);
};

/**
 * @brief Wait for an interrupt.
 *
 * The shallowest idle state; deeper ones wrap it. Returns once an interrupt is pending, even a
 * masked one. Always inlined, so it can run while the flash is powered down.
 */
static PBL_ALWAYS_INLINE void pbl_cpu_idle(void) {
  arch_cpu_idle();
}

/**
 * @brief Wait for an interrupt in the CPU's deep sleep.
 *
 * What deep sleep stops is up to the SoC. Always inlined, like pbl_cpu_idle().
 */
static PBL_ALWAYS_INLINE void pbl_cpu_idle_deep(void) {
  arch_cpu_idle_deep();
}

/** @brief Idle states, shallowest first. Defined by the SoC. */
extern const struct pbl_idle_state pbl_soc_idle_states[];

/** @brief Number of entries in @ref pbl_soc_idle_states. Defined by the SoC. */
extern const size_t pbl_soc_num_idle_states;

/**
 * @brief Get the deepest idle state allowed now.
 *
 * Implemented by the SoC. Called with every interrupt masked.
 *
 * @return Index in @ref pbl_soc_idle_states, or -1 to keep the CPU running.
 */
int pbl_soc_idle_state_limit(void);

/**
 * @brief Keep the CPU out of an idle state and of every deeper one.
 *
 * For a peripheral that does not work in the state, while it is busy. SoCs name their states, so
 * that the call reads like the datasheet:
 *
 * @code{.c}
 * pbl_idle_state_block(SOC_FOO_IDLE_STOP); // the UART does not receive in stop mode
 * prv_receive();
 * pbl_idle_state_release(SOC_FOO_IDLE_STOP);
 * @endcode
 *
 * Counted: balance each call with pbl_idle_state_release(). Can be called from ISRs. Does nothing
 * without @c CONFIG_CPU_IDLE.
 *
 * @param state Index in @ref pbl_soc_idle_states; 0 keeps the CPU running.
 */
#ifdef CONFIG_CPU_IDLE
void pbl_idle_state_block(pbl_idle_state_id_t state);
#else
static inline void pbl_idle_state_block(pbl_idle_state_id_t state) {
  (void)state;
}
#endif

/**
 * @brief Release a block taken with pbl_idle_state_block().
 *
 * @param state State passed to pbl_idle_state_block().
 */
#ifdef CONFIG_CPU_IDLE
void pbl_idle_state_release(pbl_idle_state_id_t state);
#else
static inline void pbl_idle_state_release(pbl_idle_state_id_t state) {
  (void)state;
}
#endif

/**
 * @brief Allow the CPU to idle or keep it running, e.g. while measuring.
 *
 * Idle is allowed at boot.
 *
 * @param enable False to keep the CPU out of every idle state.
 */
void pbl_idle_set_enabled(bool enable);

/**
 * @brief Get the time spent in an idle state since boot.
 *
 * @param state Index in @ref pbl_soc_idle_states.
 * @return Microseconds.
 */
uint64_t pbl_idle_residency_us(pbl_idle_state_id_t state);

/** @} */
