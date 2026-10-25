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

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
// Every boot routes, but for one that follows a routed boot which never confirmed (it died
// first): that one, and every boot after it, stay plain until a reservation.
#define TRYING_FILE "hcirouter_try"
#define OFF_FILE    "hcirouter_off"

static bool prv_exists(const char *name) {
  int fd = pfs_open(name, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  if (fd < 0) {
    return false;
  }
  (void)pfs_close(fd);
  return true;
}

static bool prv_create(const char *name) {
  int fd = pfs_open(name, OP_FLAG_WRITE, FILE_TYPE_STATIC, 1);
  if (fd < 0) {
    return false;
  }
  return pfs_close(fd) == S_SUCCESS;
}

// Whether this boot routes without a reservation.
static bool prv_auto_route(void) {
  if (prv_exists(TRYING_FILE)) {
    PBL_LOG_ERR("HCI router: the last routed boot did not come up, routing turned off");
    (void)pfs_remove(TRYING_FILE);
    (void)prv_create(OFF_FILE);
    return false;
  }
  if (prv_exists(OFF_FILE)) {
    return false;
  }
  if (!prv_create(TRYING_FILE)) {
    PBL_LOG_ERR("HCI router: cannot mark the boot, not routing");
    return false;
  }
  return true;
}

void hci_router_boot_confirm(void) {
  (void)pfs_remove(TRYING_FILE);
}
#endif

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

#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
  // A reservation turns routing on again.
  (void)pfs_remove(OFF_FILE);
#endif
  PBL_LOG_INFO("HCI router reserved for the next boot");
  return 0;
}

bool hci_router_boot_consume(void) {
  uint32_t magic = 0;

  int fd = pfs_open(RESERVATION_FILE, OP_FLAG_READ, FILE_TYPE_STATIC, 0);
  if (fd < 0) {
#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
    return prv_auto_route();
#else
    return false;
#endif
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
#ifdef CONFIG_BT_ZEPHYR_CLASSIC_AUTO
  // As a routed boot of its own, which must confirm too.
  (void)prv_create(TRYING_FILE);
#endif
  return true;
}
