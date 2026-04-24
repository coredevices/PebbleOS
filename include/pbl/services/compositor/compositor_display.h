/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file compositor_display.h
//!
//! This module handles copying the framebuffer content to the display driver.

void compositor_display_update(void (*handle_update_complete_cb)(void));

bool compositor_display_update_in_progress(void);
