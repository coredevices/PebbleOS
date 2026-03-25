/* SPDX-CopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Reconstruct a Memfault coredump from the PebbleOS flash-based coredump.
//!
//! This should be called during boot after the flash driver is initialized.
//! It reads the PebbleOS coredump from SPI flash, extracts crash registers
//! and per-thread stack memory, and saves a Memfault-format coredump to
//! RAM-backed storage for later packetization and upload.
//!
//! If no unexported PebbleOS coredump exists, or a Memfault coredump is
//! already present, this function is a no-op.
void memfault_pebble_coredump_reconstruct(void);

//! Mark the PebbleOS coredump as exported to Memfault.
//!
//! Call this after the Memfault SDK has finished reading all coredump chunks
//! (i.e. from memfault_platform_coredump_storage_clear()). This sets a flag
//! in the PebbleOS coredump's flash region header so the coredump is not
//! reconstructed again on subsequent reboots.
void memfault_pebble_coredump_mark_exported(void);

//! Allocate the RAM-backed coredump storage buffer.
//!
//! @param size The number of bytes to allocate. Use
//!   memfault_coredump_get_save_size() to compute the exact size needed.
//!
//! Called before memfault_coredump_save(). The buffer is freed when
//! memfault_platform_coredump_storage_clear() is called after all chunks
//! have been read by the packetizer.
void memfault_coredump_storage_alloc(size_t size);
