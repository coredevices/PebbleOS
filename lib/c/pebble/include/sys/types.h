/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define __need_size_t
#include <stddef.h>

// Cheap trick to make __SIZE_TYPE__ give an unsigned version
#define unsigned signed
typedef __SIZE_TYPE__ ssize_t;
#undef unsigned

typedef signed long int time_t;

typedef ssize_t off_t;
