/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <board/board.h>

/**
 * @defgroup drivers_gpio GPIO
 * @ingroup drivers
 * @brief GPIO driver interface.
 *
 * Pins are described by the board's @c OutputConfig and @c InputConfig.
 *
 * @code{.c}
 * gpio_output_init(&dev->reset_gpio, GPIO_OType_PP);
 * gpio_output_set(&dev->reset_gpio, false);
 *
 * gpio_input_init_pull_up_down(&dev->int_input, GPIO_PuPd_UP);
 * bool level = gpio_input_read(&dev->int_input);
 * @endcode
 * @{
 */

#ifdef CONFIG_SOC_NRF52
#include <hal/nrf_gpio.h>

/** @brief Output type. */
typedef enum {
  /** Push-pull. */
  GPIO_OType_PP,
  /** Open drain. */
  GPIO_OType_OD,
} GPIOOType_TypeDef;

/** @brief Input pull resistor. */
typedef enum {
  /** No pull resistor. */
  GPIO_PuPd_NOPULL,
  /** Pull-up. */
  GPIO_PuPd_UP,
  /** Pull-down. */
  GPIO_PuPd_DOWN,
} GPIOPuPd_TypeDef;

#endif

/**
 * @brief Initialize a pin as an output.
 *
 * @param pin_config Pin configuration.
 * @param otype Output type, @c GPIO_OType_PP or @c GPIO_OType_OD.
 */
void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype);

/**
 * @brief Assert or deassert an output.
 *
 * Asserting drives the pin high if @c active_high is set in @p pin_config, low otherwise.
 *
 * @param pin_config Pin configuration.
 * @param asserted True to assert.
 */
void gpio_output_set(const OutputConfig *pin_config, bool asserted);

/**
 * @brief Initialize a pin as an input without pull resistor.
 *
 * @param input_cfg Pin configuration.
 */
void gpio_input_init(const InputConfig *input_cfg);

/**
 * @brief Initialize a pin as an input with a pull resistor.
 *
 * @param input_cfg Pin configuration.
 * @param pupd Pull resistor.
 */
void gpio_input_init_pull_up_down(const InputConfig *input_cfg, GPIOPuPd_TypeDef pupd);

/**
 * @brief Read an input.
 *
 * @param input_cfg Pin configuration.
 * @return Pin level, true if high.
 */
bool gpio_input_read(const InputConfig *input_cfg);

/** @} */
