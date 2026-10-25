/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Reading and writing the SiFli SDK's struct rt_ringbuffer, as the LCPU lays it
// out in the shared voice memory: each side owns its own index and mirror, and
// changes them as one 32-bit word, as the LCPU reads them.

#include <stddef.h>
#include <stdint.h>

#include <pbl/bluetooth/lcpu_audio_probe.h>

//! Bytes in @p r.
size_t lcpu_ring_data_len(const volatile struct lcpu_audio_ring *r);

//! Free bytes in @p r.
size_t lcpu_ring_space_len(const volatile struct lcpu_audio_ring *r);

//! Take exactly @p len bytes from @p r, whose data is at @p mem; 0 if fewer are there.
size_t lcpu_ring_get(volatile struct lcpu_audio_ring *r, const volatile uint8_t *mem, uint8_t *out,
                     size_t len);

//! Copy exactly @p len bytes from @p r without taking them; 0 if fewer are there.
size_t lcpu_ring_peek(const volatile struct lcpu_audio_ring *r, const volatile uint8_t *mem,
                      uint8_t *out, size_t len);

//! Put exactly @p len bytes into @p r, whose data is at @p mem; 0 if there is no room.
size_t lcpu_ring_put(volatile struct lcpu_audio_ring *r, volatile uint8_t *mem, const uint8_t *in,
                     size_t len);
