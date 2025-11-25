/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define MHZ_TO_HZ(hz) (((uint32_t)(hz)) * 1000000)

#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

//! Convert num to a hex string and put in buffer
void itoa_hex(uint32_t num, char *buffer, int buffer_length);
