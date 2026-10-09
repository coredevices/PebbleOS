/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

bool app_message_receiver_open(size_t buffer_size);
void app_message_receiver_close(void);
