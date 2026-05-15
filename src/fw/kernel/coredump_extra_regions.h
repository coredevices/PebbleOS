/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Driver-registered RAM regions that the Memfault coredump reconstruction
//! forwards to the cloud in addition to thread stacks and log buffers.
//!
//! The Memfault port (third_party/memfault/port/src/memfault_pebble_coredump.c)
//! packages only a small slice of RAM for cloud upload by default. Drivers
//! that snapshot diagnostic state into a fixed BSS buffer ahead of a fault
//! (e.g. a peripheral register dump captured before PBL_CROAK) register that
//! buffer here so it rides along in the Memfault upload.
//!
//! Registration order:
//!   1. main.c calls coredump_extra_regions_init() early in boot.
//!   2. coredump_extra_regions_init() calls per-driver register helpers under
//!      platform conditionals.
//!   3. main.c calls pbl_analytics_init(), which triggers Memfault coredump
//!      reconstruction. The reconstruction iterates the registry via
//!      coredump_extra_regions_get() and includes each region as a cached
//!      memory region in the upload.
//!
//! Keep regions small — every byte adds to the BLE upload size. Sub-KB
//! buffers of bounded, fault-relevant state are the intended use.

#pragma once

#include <stddef.h>
#include <stdint.h>

#define COREDUMP_EXTRA_REGIONS_MAX 8

typedef struct {
  const char *name;
  uintptr_t addr;
  size_t size;
} CoredumpExtraRegion;

//! Add a region to the coredump registry. Drops silently (logged) if the
//! registry is full — bump COREDUMP_EXTRA_REGIONS_MAX if you hit that.
void coredump_extra_regions_register(const char *name, const void *addr, size_t size);

//! Iterate the registry. Out-parameter receives the number of registered
//! regions; the returned pointer is valid until the next register() call.
const CoredumpExtraRegion *coredump_extra_regions_get(size_t *out_count);

//! Called once from main.c before pbl_analytics_init() to wire up all
//! per-driver registrations under platform conditionals.
void coredump_extra_regions_init(void);
