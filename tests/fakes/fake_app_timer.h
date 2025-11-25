/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_timer.h"
#include "system/logging.h"

#include <string.h>

typedef struct FakeAppTimer {
  uint32_t timeout_ms;
  bool repeating;
  AppTimerCallback callback;
  void* callback_data;
  struct FakeAppTimer *next;
  uint32_t timer_id;
} FakeAppTimer;

static FakeAppTimer *s_fake_app_timer_head;
static uint32_t s_fake_app_timer_next_id;

static FakeAppTimer *prv_find_fake_app_timer_by_timer_id(uint32_t timer_id) {
  for (FakeAppTimer *node = s_fake_app_timer_head; node != NULL; node = node->next) {
    if (node->timer_id == timer_id) {
      return node;
    }
  }
  return NULL;
}

void fake_app_timer_init(void) {
  s_fake_app_timer_head = NULL;
  s_fake_app_timer_next_id = 0;
}

void fake_app_timer_deinit(void) {
  FakeAppTimer *timer = s_fake_app_timer_head;
  while (timer) {
    FakeAppTimer *next = timer->next;
    app_timer_cancel((AppTimer *)timer);
    timer = next;
  }
}

uint32_t fake_app_timer_get_timeout(AppTimer *timer) {
  FakeAppTimer *fake_timer = prv_find_fake_app_timer_by_timer_id((uintptr_t)timer);
  if (fake_timer) {
    return fake_timer->timeout_ms;
  }

  return 0;
}

bool fake_app_timer_is_scheduled(AppTimer *timer) {
  return (prv_find_fake_app_timer_by_timer_id((uintptr_t)timer) != NULL);
}

AppTimer* app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void* callback_data) {
  FakeAppTimer *fake_timer = malloc(sizeof(FakeAppTimer));
  *fake_timer = (FakeAppTimer) {
    .timeout_ms = timeout_ms,
    .callback = callback,
    .callback_data = callback_data,
    .next = s_fake_app_timer_head,
    .timer_id = ++s_fake_app_timer_next_id,
  };

  s_fake_app_timer_head = fake_timer;
  return (AppTimer *)(uintptr_t)fake_timer->timer_id;
}

AppTimer* app_timer_register_repeatable(uint32_t timeout_ms,
                                        AppTimerCallback callback,
                                        void* callback_data,
                                        bool repeating) {
  FakeAppTimer *fake_timer = malloc(sizeof(FakeAppTimer));
  *fake_timer = (FakeAppTimer) {
    .timeout_ms = timeout_ms,
    .repeating = repeating,
    .callback = callback,
    .callback_data = callback_data,
    .next = s_fake_app_timer_head,
    .timer_id = ++s_fake_app_timer_next_id,
  };

  s_fake_app_timer_head = fake_timer;
  return (AppTimer *)(uintptr_t)fake_timer->timer_id;
}

bool app_timer_reschedule(AppTimer *timer, uint32_t new_timeout_ms) {
  FakeAppTimer *fake_timer = prv_find_fake_app_timer_by_timer_id((uintptr_t)timer);
  if (fake_timer) {
    fake_timer->timeout_ms = new_timeout_ms;
    return true;
  }
  return false;
}

static void prv_unlink_and_free_timer(FakeAppTimer *timer) {
  FakeAppTimer *prev_timer = s_fake_app_timer_head;
  if (timer == s_fake_app_timer_head) {
    // The timer is the head
    s_fake_app_timer_head = timer->next;
  } else {
    // Not the head, find the previous one:
    while (prev_timer && prev_timer != timer &&
	   prev_timer->next && prev_timer->next != timer) {
      prev_timer = prev_timer->next;
    }
    if (!prev_timer) {
      PBL_LOG(LOG_LEVEL_ERROR, "Tried to unlink and free non-existing timer %p", timer);
      return;
    }
    prev_timer->next = timer->next;
  }
  free(timer);
}

void app_timer_cancel(AppTimer *timer) {
  FakeAppTimer *fake_timer = prv_find_fake_app_timer_by_timer_id((uintptr_t)timer);
  if (fake_timer) {
    prv_unlink_and_free_timer(fake_timer);
  }
}

bool app_timer_trigger(AppTimer *timer) {
  FakeAppTimer *fake_timer = prv_find_fake_app_timer_by_timer_id((uintptr_t)timer);
  if (fake_timer) {
    AppTimerCallback callback = fake_timer->callback;
    void *data = fake_timer->callback_data;
    if (!fake_timer->repeating) {
      prv_unlink_and_free_timer(fake_timer);
    }
    callback(data);
    return true;
  }
  return false;
}

void *app_timer_get_data(AppTimer *timer) {
  FakeAppTimer *fake_timer = prv_find_fake_app_timer_by_timer_id((uintptr_t)timer);
  if (fake_timer) {
    return fake_timer->callback_data;
  } else {
    return NULL;
  }
}

