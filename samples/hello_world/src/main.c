/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/logging/logging.h>

int main(void) {
  PBL_LOG_ALWAYS("Hello World! %s", CONFIG_BOARD);

  return 0;
}
