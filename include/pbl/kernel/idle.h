/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>
#include <pbl/kernel/types.h>

#include <pbl_arch_idle.h>

/**
 * @defgroup kernel_idle Idle states
 * @ingroup kernel
 * @brief Low-power states the CPU idles in, provided by the SoC.
 *
 * When nothing is runnable, the kernel picks the deepest state that no driver blocks and is worth
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
 * const size_t pbl_soc_num_idle_states = PBL_ARRAY_SIZE(pbl_soc_idle_states);
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

/**
 * @brief Idle states, shallowest first.
 *
 * Defined by SoCs that select @c CONFIG_SOC_HAS_IDLE_STATES; for the others the kernel defines a
 * single state that calls pbl_cpu_idle().
 */
extern const struct pbl_idle_state pbl_soc_idle_states[];

/** @brief Number of entries in @ref pbl_soc_idle_states. */
extern const size_t pbl_soc_num_idle_states;

/** @brief Times of an idle hook, see pbl_idle_hook_set_times(). */
struct pbl_idle_hook_times {
  /**
   * Shortest time until the next timeout worth calling the hook for, in ticks; 0 until set, and
   * the hook is not called meanwhile.
   */
  pbl_tick_t min_ticks;
  /** Time the exit function takes, in ticks; the CPU is woken up this much earlier. */
  pbl_tick_t exit_ticks;
};

/** @brief Idle hook, see @ref PBL_IDLE_HOOK_DEFINE. */
struct pbl_idle_hook {
  /** Called before the CPU enters a state, may be NULL. */
  void (*enter)(pbl_idle_state_id_t state, void *arg);
  /** Called after the CPU left a state, may be NULL. */
  void (*exit)(pbl_idle_state_id_t state, void *arg);
  /** Passed to the functions. */
  void *arg;
  /** Times, NULL for a late hook. */
  struct pbl_idle_hook_times *times;
};

/**
 * @def PBL_IDLE_HOOK_DEFINE
 * @brief Define a hook, for a driver to act before the CPU idles and after.
 *
 * The kernel calls the hooks around the idle states once their times are set with
 * pbl_idle_hook_set_times(), when the time until the next timeout is at least the hook's minimum,
 * and wakes the CPU up earlier by the time its exit function takes. It tells the state in
 * @c state, an index in @ref pbl_soc_idle_states, for a hook that only acts around some. Hooks
 * run with every interrupt masked: they must be short and cannot block. On the way out they run
 * in the opposite order.
 *
 * @code{.c}
 * static void prv_idle_enter(pbl_idle_state_id_t state, void *arg) {
 *   prv_power_down(arg);
 * }
 *
 * PBL_IDLE_HOOK_DEFINE(foo, prv_idle_enter, prv_idle_exit, &s_foo);
 *
 * static int prv_init(void) {
 *   // Powering back up takes 2 ms, which pays off from 5 ms on
 *   pbl_idle_hook_set_times(PBL_IDLE_HOOK(foo), 5000, 2000);
 *   return 0;
 * }
 * @endcode
 *
 * Hooks are not called without @c CONFIG_CPU_IDLE.
 *
 * @param _name Identifier unique among hooks.
 * @param _enter Function called before entering a state, or NULL.
 * @param _exit Function called after leaving a state, or NULL.
 * @param _arg Argument of the functions.
 */

/**
 * @def PBL_IDLE_HOOK_LATE_DEFINE
 * @brief Define a hook the SoC calls last before the CPU idles and first after.
 *
 * For what must happen once the SoC took down what code needs to run from flash, where a state
 * does. The SoC calls late hooks from the innermost part of such states. The functions, and all
 * they call, must be @ref PBL_SECTION_RAM or inline. Otherwise as @ref PBL_IDLE_HOOK_DEFINE,
 * without times: they belong to the state.
 */
#define PBL_IDLE_HOOK_DEFINE(_name, _enter, _exit, _arg)                        \
  static struct pbl_idle_hook_times pbl_idle_hook_times_##_name;                \
  static const struct pbl_idle_hook pbl_idle_hook_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_idle_hook, idle_hooks, #_name) = {                       \
    .enter = (_enter),                                                          \
    .exit = (_exit),                                                            \
    .arg = (_arg),                                                              \
    .times = &pbl_idle_hook_times_##_name,                                      \
  }
#define PBL_IDLE_HOOK_LATE_DEFINE(_name, _enter, _exit, _arg)                        \
  static const struct pbl_idle_hook pbl_idle_hook_late_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_idle_hook, idle_late,                                         \
      #_name) = {.enter = (_enter), .exit = (_exit), .arg = (_arg)}

/**
 * @brief Get a hook defined with @ref PBL_IDLE_HOOK_DEFINE in the same file.
 *
 * @param _name Name of the hook.
 */
#define PBL_IDLE_HOOK(_name) (&pbl_idle_hook_##_name)

/**
 * @brief Set the times of a hook, rounded up to ticks.
 *
 * A driver sets them once it knows them, e.g. from the part it found; the hook is not called
 * before.
 *
 * @param hook Hook, see @ref PBL_IDLE_HOOK.
 * @param min_us Shortest time until the next timeout worth calling the hook for, in microseconds.
 * @param exit_us Time the exit function takes, in microseconds.
 */
void pbl_idle_hook_set_times(const struct pbl_idle_hook *hook, uint32_t min_us, uint32_t exit_us);

/**
 * @brief Call the enter function of every late hook. Runs from RAM.
 *
 * @param state Index in @ref pbl_soc_idle_states.
 */
void pbl_idle_hooks_late_enter(pbl_idle_state_id_t state);

/**
 * @brief Call the exit function of every late hook. Runs from RAM.
 *
 * @param state Index in @ref pbl_soc_idle_states.
 */
void pbl_idle_hooks_late_exit(pbl_idle_state_id_t state);

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

/** @brief Request on the time the CPU takes to wake up, see pbl_idle_latency_request_add(). */
struct pbl_idle_latency_request {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_idle_latency_request *next;
  pbl_tick_t ticks;
  /** @endcond */
};

/**
 * @brief Keep the CPU out of the idle states, and off the hooks, that take longer to wake up from.
 *
 * For code that must run within some time of an interrupt, whatever the state the CPU idles in,
 * as opposed to a peripheral that does not work in a state, see pbl_idle_state_block(). The
 * shortest request applies. Can be called from ISRs. Does nothing without @c CONFIG_CPU_IDLE.
 *
 * @code{.c}
 * static struct pbl_idle_latency_request s_request;
 *
 * pbl_idle_latency_request_add(&s_request, 500); // the radio is served within 500 us
 * prv_radio_event();
 * pbl_idle_latency_request_remove(&s_request);
 * @endcode
 *
 * @param req Request, not added yet. Must stay valid until removed.
 * @param us Longest time to wake up tolerated, in microseconds.
 */
#ifdef CONFIG_CPU_IDLE
void pbl_idle_latency_request_add(struct pbl_idle_latency_request *req, uint32_t us);
#else
static inline void pbl_idle_latency_request_add(struct pbl_idle_latency_request *req, uint32_t us) {
  (void)req;
  (void)us;
}
#endif

/**
 * @brief Remove a request added with pbl_idle_latency_request_add().
 *
 * @param req Request.
 */
#ifdef CONFIG_CPU_IDLE
void pbl_idle_latency_request_remove(struct pbl_idle_latency_request *req);
#else
static inline void pbl_idle_latency_request_remove(struct pbl_idle_latency_request *req) {
  (void)req;
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
