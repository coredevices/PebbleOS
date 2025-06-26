/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! @file exti.inl.h
//!
//! Helper functions intended to be inlined into the calling code.

#if !defined(MICRO_FAMILY_NRF5) && !defined(MICRO_FAMILY_SF32LB52)
static inline void exti_enable(ExtiConfig config) {
  exti_enable_other(config.exti_line);
}

static inline void exti_disable(ExtiConfig config) {
  exti_disable_other(config.exti_line);
}
#endif

