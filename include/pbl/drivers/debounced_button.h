/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup drivers_debounced_button Debounced buttons
 * @ingroup drivers
 * @brief Debounced button events.
 *
 * Samples the buttons on a timer while any of them changes state and posts
 * @c PEBBLE_BUTTON_DOWN_EVENT / @c PEBBLE_BUTTON_UP_EVENT events once a new state is stable.
 * Holding the reset combination (back and select) resets the device, except in MFG builds.
 * @{
 */

/**
 * @brief Initialize the buttons and start generating button events.
 *
 * Calls button_init().
 */
void debounced_button_init(void);

/** @} */
