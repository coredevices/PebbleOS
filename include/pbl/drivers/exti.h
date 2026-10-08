/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <board/board.h>

/**
 * @defgroup drivers_exti External interrupts
 * @ingroup drivers
 * @brief GPIO edge interrupts.
 *
 * The pin is described by the board's @c ExtiConfig.
 *
 * @code{.c}
 * static void prv_int_handler(void) {
 *   system_task_add_callback_from_isr(prv_handle_int, NULL);
 * }
 *
 * exti_configure_pin(dev->int_exti, ExtiTrigger_Falling, prv_int_handler);
 * exti_enable(dev->int_exti);
 * @endcode
 * @{
 */

/** @brief Edge that triggers the interrupt. */
typedef enum {
  /** Rising edge. */
  ExtiTrigger_Rising,
  /** Falling edge. */
  ExtiTrigger_Falling,
  /** Both edges. */
  ExtiTrigger_RisingFalling
} ExtiTrigger;

/** @brief Interrupt handler, called in ISR context. */
typedef void (*ExtiHandlerCallback)(void);

/**
 * @brief Enable the interrupt of a configured pin.
 *
 * @param config Pin configured with exti_configure_pin().
 */
void exti_enable(ExtiConfig config);

/**
 * @brief Disable the interrupt of a pin.
 *
 * @param config Pin configured with exti_configure_pin().
 */
void exti_disable(ExtiConfig config);

/**
 * @brief Configure a pin as an interrupt source and register its handler.
 *
 * The interrupt is delivered once enabled with exti_enable().
 *
 * @param cfg Pin to configure.
 * @param trigger Edge that triggers the interrupt.
 * @param cb Handler, called in ISR context.
 */
void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);

/** @} */