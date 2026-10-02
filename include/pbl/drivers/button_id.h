/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @addtogroup UI
 * @{
 */

/**
 * @addtogroup Clicks
 * @brief Dealing with button input
 * @{
 */

/**
 * @brief Button ID values
 * @see click_recognizer_get_button_id()
 */
typedef enum {
  /** Back button */
  BUTTON_ID_BACK = 0,
  /** Up button */
  BUTTON_ID_UP,
  /** Select (middle) button */
  BUTTON_ID_SELECT,
  /** Down button */
  BUTTON_ID_DOWN,
  /** Total number of buttons */
  NUM_BUTTONS
} ButtonId;

/** @} */

/** @} */
