/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "button_id.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_button Buttons
 * @ingroup drivers
 * @brief Raw button state.
 *
 * Reads the button inputs directly, without debouncing; see @ref drivers_debounced_button for
 * button events.
 * @{
 */

/** @brief Configure the button inputs. */
void button_init(void);

/**
 * @brief Set whether the device is rotated by 180 degrees.
 *
 * While rotated, the up and down buttons are swapped.
 *
 * @param rotated True if rotated by 180 degrees.
 */
void button_set_rotated(bool rotated);

/**
 * @brief Check whether a button is pressed.
 *
 * @param id Button to check.
 * @return True if pressed.
 */
bool button_is_pressed(ButtonId id);

/**
 * @brief Get the state of all buttons.
 *
 * @return Bitmask with bit @c n set if the button with ButtonId @c n is pressed.
 */
uint8_t button_get_state_bits(void);

/** @} */
