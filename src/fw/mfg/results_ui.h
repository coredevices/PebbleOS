/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window.h"
#include "applib/ui/text_layer.h"
#include "mfg/mfg_info.h"

typedef void (*MfgResultsCallback)(void);

typedef struct {
  MfgTest test;

  TextLayer pass_text_layer;
  TextLayer fail_text_layer;

  MfgResultsCallback results_cb;
} MfgResultsUI;

void mfg_results_ui_init(MfgResultsUI *results_ui, MfgTest test, Window *window);
void mfg_results_ui_set_callback(MfgResultsUI *ui, MfgResultsCallback cb);
