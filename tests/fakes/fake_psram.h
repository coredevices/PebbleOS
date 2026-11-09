/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/psram.h>

#include <clar.h>

static unsigned int s_fake_psram_holds;
static uint32_t s_fake_psram_epoch;

void psram_retain_acquire(void) {
  s_fake_psram_holds++;
}

void psram_retain_release(void) {
  cl_assert(s_fake_psram_holds > 0);
  s_fake_psram_holds--;
}

bool psram_is_retained(void) {
  return s_fake_psram_holds > 0;
}

uint32_t psram_epoch(void) {
  return s_fake_psram_epoch;
}

//! Number of retention holds taken.
static inline unsigned int fake_psram_holds(void) {
  return s_fake_psram_holds;
}

//! Simulate a deep sleep with no hold: the epoch changes. The caller clobbers the external RAM it
//! owns to simulate the loss of contents.
static inline void fake_psram_power_down(void) {
  cl_assert_equal_i(s_fake_psram_holds, 0);
  s_fake_psram_epoch++;
}

static inline void fake_psram_reset(void) {
  s_fake_psram_holds = 0;
  s_fake_psram_epoch = 0;
}
