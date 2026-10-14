/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/progress_window.h>

PBL_WEAK void progress_window_init(ProgressWindow *data) {
}

PBL_WEAK void progress_window_deinit(ProgressWindow *data) {
}

PBL_WEAK ProgressWindow *progress_window_create(void) {
  return nullptr;
}

PBL_WEAK void progress_window_destroy(ProgressWindow *window) {
}

PBL_WEAK void progress_window_push(ProgressWindow *window, WindowStack *window_stack) {
}

PBL_WEAK void app_progress_window_push(ProgressWindow *window) {
}

PBL_WEAK void progress_window_pop(ProgressWindow *window) {
}

PBL_WEAK void progress_window_set_max_fake_progress(ProgressWindow *window,
                                                    int16_t max_fake_progress) {
}

PBL_WEAK void progress_window_set_progress(ProgressWindow *window, int16_t progress) {
}

PBL_WEAK void progress_window_set_result_success(ProgressWindow *window) {
}

PBL_WEAK void progress_window_set_result_failure(ProgressWindow *window, uint32_t timeline_res_id,
                                                 const char *message, uint32_t delay) {
}

PBL_WEAK void progress_window_set_callbacks(ProgressWindow *window,
                                            ProgressWindowCallbacks callbacks, void *context) {
}

PBL_WEAK void progress_window_set_back_disabled(ProgressWindow *window, bool disabled) {
}
