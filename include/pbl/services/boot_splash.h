/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_boot_splash Boot splash
 * @ingroup services
 * @brief Splash screen shown while the firmware boots.
 *
 * With @c CONFIG_PBLBOOT or on QEMU, the firmware draws the logo with an animated progress bar
 * from its own task. Elsewhere the bootloader shows the splash and these functions do nothing.
 * @{
 */

/**
 * @brief Start the boot splash screen.
 *
 * Initializes the display and starts the animation task.
 */
void boot_splash_start(void);

/**
 * @brief Stop the boot splash screen.
 *
 * Call when the compositor is ready to take over the display. Waits for the animation task to
 * exit and leaves the logo on screen without the progress bar.
 */
void boot_splash_stop(void);

/** @} */
