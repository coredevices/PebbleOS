/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_compositor_compositor_display Display flush
 * @ingroup services_compositor
 * @brief Copies the dirty region of the system framebuffer to the display driver.
 * @{
 */

/**
 * @brief Flush the dirty region of the system framebuffer to the display.
 *
 * Does nothing when the framebuffer is not dirty. On completion the dirty region is reset.
 *
 * @param handle_update_complete_cb Called once the update completes, or NULL.
 */
void compositor_display_update(void (*handle_update_complete_cb)(void));

/**
 * @brief Check whether a display update is in progress.
 *
 * @return true while the display driver is still consuming the framebuffer.
 */
bool compositor_display_update_in_progress(void);

/** @} */
