/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services.h"

int pbl_bt__services_init(const struct pbl_bt_dis_info *dis_info) {
  [[maybe_unused]] int rc;

  // The order sets the attribute handles, which peers cache: there is no
  // Database Hash, so keep it stable.
#ifdef CONFIG_BT_SERVICE_DIS
  pbl_bt__dis_init(dis_info);
#endif
#ifdef CONFIG_BT_SERVICE_PPS
  rc = pbl_bt__pps_init();
  if (rc != 0) {
    return rc;
  }
#endif
#ifdef CONFIG_BT_SERVICE_BAS
  pbl_bt__bas_init();
#endif
#ifdef CONFIG_BT_SERVICE_PPOG_REVERSED
  rc = pbl_bt__ppog_reversed_init();
  if (rc != 0) {
    return rc;
  }
#endif

  return 0;
}
