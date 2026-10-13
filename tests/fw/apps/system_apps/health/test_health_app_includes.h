/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/size.h>

#include <applib/ui/window_private.h>
#include <clar.h>

// Fakes
/////////////////////

#include <fake_content_indicator.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fixtures/load_test_resources.h>

// Stubs
/////////////////////

#include <stubs_activity.h>
#include <stubs_animation_timing.h>
#include <stubs_app_launch_reason.h>
#include <stubs_app_window_stack.h>
#include <stubs_health_service.h>
#include <stubs_pebble_process_info.h>
#include <stubs_status_bar_layer.h>

// Helper Functions
/////////////////////

#include <fw/graphics/util.h>
