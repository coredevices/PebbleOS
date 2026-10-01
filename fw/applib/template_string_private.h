/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/template_string.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct TemplateStringState {
  const char *position;
  char *output;
  size_t output_remaining;
  TemplateStringEvalConditions *eval_cond;
  const TemplateStringVars *vars;
  TemplateStringError *error;

  intmax_t filter_state;
  //! Set to true when the filter_state was set by `time_until`, false for `time_since`.
  bool time_was_until;
  bool filters_complete;
} TemplateStringState;
