/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <board/display.h>
#include <applib/graphics/gtypes.h>

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup drivers_display Display
 * @ingroup drivers
 * @brief Display driver interface.
 *
 * Updates are row based: the driver pulls the rows to send from a callback and reports
 * completion through another one.
 *
 * @code{.c}
 * static bool prv_next_row(DisplayRow *row) {
 *   if (s_y >= s_y_end) {
 *     return false;
 *   }
 *   row->address = s_y;
 *   row->data = framebuffer_get_line(fb, s_y++);
 *   return true;
 * }
 *
 * display_update(prv_next_row, prv_update_done);
 * @endcode
 * @{
 */

/** @brief Row to send to the display. */
typedef struct {
  /** Row index, from 0 at the top. */
  uint16_t address;
  /** Row pixels in framebuffer format; must stay valid until the update completes. */
  uint8_t *data;
} DisplayRow;

/**
 * @brief Provide the next row of an update.
 *
 * @param[out] row Next row to send.
 * @return True if @p row was filled, false when there are no more rows.
 */
typedef bool (*NextRowCallback)(DisplayRow *row);

/** @brief Notification that an update has completed. */
typedef void (*UpdateCompleteCallback)(void);

/**
 * @brief Show a boot animation frame.
 *
 * Lets the boot animation draw before the compositor is initialized.
 *
 * @param framebuffer Full frame of @c DISPLAY_FRAMEBUFFER_BYTES bytes.
 */
void display_update_boot_frame(uint8_t *framebuffer);

/** @brief Initialize the display. */
void display_init(void);

/** @brief Clear the display, where supported. */
void display_clear(void);

/**
 * @brief Turn the display on or off.
 *
 * @param enabled True to turn it on.
 */
void display_set_enabled(bool enabled);

/**
 * @brief Set whether the image is rotated by 180 degrees.
 *
 * @param rotated True to rotate by 180 degrees.
 */
void display_set_rotated(bool rotated);

/**
 * @brief Start a display update.
 *
 * Pulls rows from @p nrcb until it returns false, sends them, and calls @p uccb once done. Must
 * not be called while an update is in progress.
 *
 * @param nrcb Provides the rows to update.
 * @param uccb Called when the update has completed.
 */
void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb);

/**
 * @brief Check whether an update is in progress.
 *
 * @return True between display_update() and its completion.
 */
bool display_update_in_progress(void);

/** @} */