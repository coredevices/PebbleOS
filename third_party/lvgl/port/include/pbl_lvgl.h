/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <lvgl.h>

/**
 * @file
 * @brief LVGL on the display and the input devices of the board.
 *
 * The port renders in RGB565 and converts to the format of the display, feeds the touchscreen to
 * a pointer and the keys to a keypad, and uses the kernel uptime as the tick. LVGL is not
 * thread-safe: every call into it, lv_timer_handler() included, comes from one thread.
 *
 * @code{.c}
 * ret = pbl_lvgl_init();
 *
 * lv_label_set_text(lv_label_create(lv_screen_active()), "Hello");
 *
 * while (true) {
 *   pbl_sleep_ms(lv_timer_handler());
 * }
 * @endcode
 */

/**
 * @brief Initialize LVGL, its display and its input devices.
 *
 * @return 0, @c -ENOMEM without memory for the buffers or another negative errno.
 */
int pbl_lvgl_init(void);

/**
 * @brief Get the LVGL display of the board's display.
 *
 * @return Display, or NULL before pbl_lvgl_init().
 */
lv_display_t *pbl_lvgl_get_display(void);

/**
 * @brief Get the pointer fed by the touchscreen.
 *
 * @return Input device, or NULL without @c CONFIG_TOUCH.
 */
lv_indev_t *pbl_lvgl_get_pointer(void);

/**
 * @brief Get the keypad fed by the keys.
 *
 * Up and Down are @c LV_KEY_PREV and @c LV_KEY_NEXT, Select is @c LV_KEY_ENTER and Back is
 * @c LV_KEY_ESC. The keypad drives the default group, which the port creates.
 *
 * @return Input device, or NULL without @c CONFIG_INPUT.
 */
lv_indev_t *pbl_lvgl_get_keypad(void);
