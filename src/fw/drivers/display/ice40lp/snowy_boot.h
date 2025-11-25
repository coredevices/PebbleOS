/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>

//! Functions for controlling the display FPGA in bootloader mode, such as
//! early in the boot process before it is reconfigured in framebuffer mode.
//!
//! These functions all assume that all necessary GPIOs and the SPI peripheral
//! are configured correctly, and that the bootloader is already in bootloader
//! mode.

//! Display the Pebble logo and turn on the screen.
void boot_display_show_boot_splash(void);

//! Show the Pebble logo with a progress bar.
void boot_display_show_firmware_update_progress(
    uint32_t numerator, uint32_t denominator);

//! Show a sad-watch error.
bool boot_display_show_error_code(uint32_t error_code);

//! Black out the screen and prepare for power down.
void boot_display_screen_off(void);
