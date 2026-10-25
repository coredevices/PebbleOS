/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// One-shot enable of the HCI router. The reservation is a PFS file, read and
// deleted before the controller is powered, so a boot that dies in Bluetooth
// start-up (where the button driver is not running yet) comes back on the
// plain transport after one reset.

#include <stdint.h>

#include <pbl/bluetooth/hci_router.h>
#include <pbl/logging/logging.h>
#include <pbl/services/filesystem/pfs.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

#define RESERVATION_FILE  "hcirouter"
#define RESERVATION_MAGIC 0x48435254UL // "HCRT"

int hci_router_boot_reserve(void) {
  const uint32_t magic = RESERVATION_MAGIC;

  (void)pfs_remove(RESERVATION_FILE);
  int fd = pfs_open(RESERVATION_FILE, OP_FLAG_WRITE, FILE_TYPE_STATIC, sizeof(magic));
  if (fd < 0) {
    PBL_LOG_ERR("HCI router: cannot create reservation (%d)", fd);
    return fd;
  }

  int rc = pfs_write(fd, &magic, sizeof(magic));
  status_t close_rc = pfs_close(fd);
  if (rc != (int)sizeof(magic) || close_rc != S_SUCCESS) {
    PBL_LOG_ERR("HCI router: cannot write reservation (%d, %d)", rc, (int)close_rc);
    (void)pfs_remove(RESERVATION_FILE);
    return -1;
  }

  PBL_LOG_INFO("HCI router reserved for the next boot");
  return 0;
}

bool hci_router_boot_consume(void) {
  uint32_t magic = 0;

  int fd = pfs_open(RESERVATION_FILE, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  if (fd < 0) {
    return false;
  }
  int rc = pfs_read(fd, &magic, sizeof(magic));
  (void)pfs_close(fd);

  // Only route when the reservation is really gone; otherwise every boot would.
  status_t rm = pfs_remove(RESERVATION_FILE);
  if (rm != S_SUCCESS) {
    PBL_LOG_ERR("HCI router: cannot delete reservation (%d), not routing", (int)rm);
    return false;
  }

  if (rc != (int)sizeof(magic) || magic != RESERVATION_MAGIC) {
    PBL_LOG_WRN("HCI router: bad reservation ignored");
    return false;
  }

  PBL_LOG_INFO("HCI router enabled for this boot");
  return true;
}
