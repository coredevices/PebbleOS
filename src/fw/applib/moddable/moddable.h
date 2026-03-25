/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>

//! Flag to enable XS instrumentation logging over Bluetooth.
//! When set, the Moddable XS engine will log instrumentation data (e.g. memory
//! usage, slot/chunk/stack statistics) via app_log. Logging is only active when
//! a Bluetooth log listener is connected; otherwise this flag has no effect.
//! @see ModdableCreationRecord
#define kModdableCreationFlagLogInstrumentation  (1 << 0)

//! Configuration record for creating a Moddable XS virtual machine.
//! Used with moddable_createMachine() to customize the JS runtime.
//! Set recordSize to sizeof(ModdableCreationRecord) for version compatibility.
typedef struct {
	uint32_t		recordSize;	//!< Size of this struct in bytes (for versioning)

	uint32_t		stack;		//!< Stack size in bytes (0 for default)
	uint32_t		slot;		//!< Slot heap size in bytes (0 for default)
	uint32_t		chunk;		//!< Chunk heap size in bytes (0 for default)
	uint32_t		flags;		//!< Combination of kModdableCreationFlag* values
} ModdableCreationRecord;

//! Create and start a Moddable XS virtual machine for an Alloy app.
//! @param creation Configuration record, or NULL for default settings.
//! @see ModdableCreationRecord
void moddable_createMachine(ModdableCreationRecord *creation);
