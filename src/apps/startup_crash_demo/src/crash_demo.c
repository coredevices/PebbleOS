/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

int main(void) {
  void (*foobar)(void) = NULL;
  foobar();

  app_event_loop();
}

