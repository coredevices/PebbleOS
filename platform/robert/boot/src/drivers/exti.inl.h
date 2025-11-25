/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file exti.inl.h
//!
//! Helper functions intended to be inlined into the calling code.

static inline void exti_enable(ExtiConfig config) {
  exti_enable_other(config.exti_line);
}

static inline void exti_disable(ExtiConfig config) {
  exti_disable_other(config.exti_line);
}
