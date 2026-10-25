/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Stand-in for the firmware header, with what ctl/zbt_ctl.c uses.
#pragma once
typedef void (*CallbackEventCallback)(void *data);
void launcher_task_add_callback(CallbackEventCallback callback, void *data);
