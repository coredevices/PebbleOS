/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Start the boot splash screen.
//! On platforms with software-rendered splash (PBLBOOT), this shows
//! an animated splash with a spinning indicator. On other platforms, this
//! calls the display driver's splash screen function
void boot_splash_start(void);

//! Stop the boot splash screen.
//! This should be called when the compositor is ready to take over the display.
//! On platforms with animated splash, this stops the animation task.
//! On other platforms, this is a no-op.
void boot_splash_stop(void);
