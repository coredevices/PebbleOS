/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef OS_TIMEOUT_NEVER
#define OS_TIMEOUT_NEVER UINT32_MAX
#endif
#ifndef OS_WAIT_FOREVER
#define OS_WAIT_FOREVER UINT32_MAX
#endif

struct trng_dev;

void *os_dev_open(const char *devname, uint32_t timo, void *arg);

size_t trng_read(struct trng_dev *trng, void *ptr, size_t size);
