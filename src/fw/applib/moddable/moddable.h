/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>

typedef struct {
	uint32_t		recordSize;

	uint32_t		stack;		// bytes
	uint32_t		slot;			// bytes
	uint32_t		chunk;		// bytes
} ModdableCreationRecord;

void moddable_createMachine(ModdableCreationRecord *creation);