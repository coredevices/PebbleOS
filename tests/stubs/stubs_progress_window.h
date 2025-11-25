/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/progress_window.h"

void progress_window_init(ProgressWindow *data) {};

void progress_window_deinit(ProgressWindow *data) {};

ProgressWindow *progress_window_create(void) {return NULL;};

void progress_window_destroy(ProgressWindow *window) {};

void progress_window_push(ProgressWindow *window, WindowStack *window_stack) {};

void app_progress_window_push(ProgressWindow *window) {};

void progress_window_pop(ProgressWindow *window) {};

void progress_window_set_max_fake_progress(ProgressWindow *window, int16_t max_fake_progress) {};

void progress_window_set_progress(ProgressWindow *window, int16_t progress) {};

void progress_window_set_result_success(ProgressWindow *window) {};

void progress_window_set_result_failure(ProgressWindow *window, uint32_t timeline_res_id,
                                          const char *message, uint32_t delay) {};

void progress_window_set_callbacks(ProgressWindow *window, ProgressWindowCallbacks callbacks,
                                   void *context) {};

void progress_window_set_back_disabled(ProgressWindow *window, bool disabled) {};
