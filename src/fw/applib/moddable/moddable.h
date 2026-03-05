/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>

#define kModdableCreationFlagLogInstrumentation  (1 << 0)

typedef struct {
	uint32_t		recordSize;

	uint32_t		stack;		// bytes
	uint32_t		slot;			// bytes
	uint32_t		chunk;		// bytes
	uint32_t		flags;
} ModdableCreationRecord;

void moddable_createMachine(ModdableCreationRecord *creation);