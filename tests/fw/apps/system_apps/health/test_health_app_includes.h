/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/window_private.h"
#include "util/size.h"

#include "clar.h"

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

// Stubs
/////////////////////

#include "stubs_activity.h"
#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_launch_reason.h"
#include "stubs_app_timer.h"
#include "stubs_app_window_stack.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_health_service.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_info.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Helper Functions
/////////////////////

#include "fw/graphics/util.h"
