/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The controller of the two-direction test, with numbers that each serve one direction: the
// functions need 5, it has 3
#define FAKE_NUM_EPS 4U
#define FAKE_CAPS    PBL_UDC_CAP_EP_ONE_DIR

#include "usbd_numbering.h"

void test_usbd_numbering_overflow__enable(void) {
  cl_assert_equal_i(pbl_usbd_enable(), -ENOSPC);
  cl_assert(!s_connected);
  cl_assert(s_cb == nullptr);

  cl_assert_equal_i(pbl_usbd_enable(), -ENOSPC);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(a_vendor), 0x01), 0xff);
}
