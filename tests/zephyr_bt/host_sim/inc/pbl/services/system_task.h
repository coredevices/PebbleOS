/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
#include <stdbool.h>
typedef void (*SystemTaskEventCallback)(void *data);
bool system_task_add_callback(SystemTaskEventCallback cb, void *data);
bool system_task_add_callback_droppable(SystemTaskEventCallback cb, void *data);
