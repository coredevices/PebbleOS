/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/input/input.h>

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_input_callback, input_cbs);

void pbl_input_report(enum pbl_input_type type, uint16_t code, int32_t value, bool sync) {
  const struct pbl_input_event evt = {
    .type = type,
    .sync = sync,
    .code = code,
    .value = value,
  };

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_input_callback, input_cbs, cb) {
    cb->callback(&evt, cb->user_data);
  }
}
