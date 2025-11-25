/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// Take a protobuf buffer and decode it with the command line tool
void protobuf_log_test_parse_protoc(uint8_t *msg);

