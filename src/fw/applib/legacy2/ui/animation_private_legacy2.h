/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"

#include "syscall/syscall.h"

typedef struct {
  ListNode *head; //! Pointer to the Animation struct that is the animation that is scheduled
                  //! first.
  AppTimer* timer_handle;

  //! The delay the animation scheduler uses between finishing a frame and starting a new one.
  //! Derived from actual rendering/calculation times, using a PID-like control algorithm.
  uint32_t last_delay_ms;
  uint32_t last_frame_time; //! Absolute RTC time of the moment the last animation frame started.
} AnimationLegacy2Scheduler;

void animation_legacy2_private_init_scheduler(AnimationLegacy2Scheduler* scheduler);

void animation_legacy2_private_unschedule_all(AppTaskCtxIdx app_task_ctx_idx);
