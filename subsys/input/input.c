/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/input/input.h>

#ifndef PBL_NO_LINKER_SCRIPT
extern const struct pbl_input_callback __pbl_input_callbacks_start[];
extern const struct pbl_input_callback __pbl_input_callbacks_end[];
#else
extern const struct pbl_input_callback __pbl_input_callbacks_start[] PBL_UNSORTED_SECTION_START(
    pbl_input_cb);
extern const struct pbl_input_callback __pbl_input_callbacks_end[] PBL_UNSORTED_SECTION_END(
    pbl_input_cb);
#endif

void pbl_input_report(enum pbl_input_type type, uint16_t code, int32_t value, bool sync) {
  const struct pbl_input_event evt = {
    .type = type,
    .sync = sync,
    .code = code,
    .value = value,
  };

  for (const struct pbl_input_callback *cb = __pbl_input_callbacks_start;
       cb < __pbl_input_callbacks_end; cb++) {
    cb->callback(&evt, cb->user_data);
  }
}
