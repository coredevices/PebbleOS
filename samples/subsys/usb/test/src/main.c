/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/usb/usbd.h>

int main(void) {
  int ret;

  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "usb: test function failed (%d)\n", ret);
    return ret;
  }

  printf("usb: test function up\n");

  return 0;
}
