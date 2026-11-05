/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/display.h>
#include <pbl/drivers/panel/ls013b7dd02.h>

static const struct pbl_display s_display = {
  .width = 260,
  .height = 260,
  .round_mask = pbl_panel_ls013b7dd02_round_mask,
};

const struct pbl_display *const DISPLAY = &s_display;
