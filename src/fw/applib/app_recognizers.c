/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "app_recognizers.h"

#include "process_state/app_state/app_state.h"

void app_recognizers_attach_recognizer(Recognizer *recognizer) {
  recognizer_add_to_list(recognizer, app_state_get_recognizer_list());
}

void app_recognizers_detach_recognizer(Recognizer *recognizer) {
  recognizer_remove_from_list(recognizer, app_state_get_recognizer_list());
}
