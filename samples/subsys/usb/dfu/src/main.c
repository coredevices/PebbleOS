/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/board/partitions.h>
#include <pbl/usb/dfu.h>
#include <pbl/usb/usbd.h>

/* The partition the host downloads to, from the board overlay of the sample. */
PBL_USB_DFU_REGION_DEFINE(image, PART_DFU);

int main(void) {
  int ret;

  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "dfu: USB failed (%d)\n", ret);
    return ret;
  }

  /* The device now serves the host from the controller's interrupt and the system work queue. */
  printf("dfu: ready, partition of %d KiB at 0x%x\n", (int)(PART_DFU_SIZE / 1024U),
         (unsigned int)PART_DFU_OFFSET);

  return 0;
}
