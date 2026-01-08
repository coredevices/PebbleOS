/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "syscall/syscall.h"

// App analytics syscalls - stubs for test builds
void sys_app_pp_app_message_analytics_count_sent(void) {}
void sys_app_pp_app_message_analytics_count_received(void) {}
uint32_t sys_app_pp_app_message_get_sent_count(void) { return 0; }
uint32_t sys_app_pp_app_message_get_received_count(void) { return 0; }
