/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/drivers/disk.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup usb_msc USB mass storage
 * @ingroup usbd
 * @brief Mass storage, Bulk-Only Transport with the SCSI transparent command set: the host reads
 * and writes disks of the device as its own.
 *
 * The function has one interface and a bulk endpoint in each direction, numbered by the stack
 * among the other functions'. Each disk is a logical unit the application defines with
 * PBL_USB_MSC_LUN_DEFINE(), in the order of their names, removable media the host can eject. The
 * disks are read and written on the system work queue, never from the controller's interrupt,
 * through two buffers so that the bus moves one while the disk fills or empties the other.
 *
 * @code{.c}
 * PBL_USB_MSC_LUN_DEFINE(ram, &pbl_disk_ram, false);
 *
 * ret = pbl_usbd_enable();
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
struct pbl_usb_msc__lun_state {
  uint8_t sense_key;
  uint8_t asc;
  uint8_t ascq;
  bool ejected;
  bool prevent;
};
/** @endcond */

/** @brief A logical unit: a disk the host sees. */
struct pbl_usb_msc_lun {
  /** Disk. */
  const struct pbl_disk *disk;
  /** Whether the host may only read it, whatever the disk takes. */
  bool read_only;
  /** @cond INTERNAL_HIDDEN */
  struct pbl_usb_msc__lun_state *state;
  /** @endcond */
};

/**
 * @brief Define a logical unit of the function.
 *
 * Up to 16 logical units are supported, numbered in the order of their names.
 *
 * @param _name Name of the logical unit.
 * @param _disk Disk.
 * @param _read_only Whether the host may only read it.
 */
#define PBL_USB_MSC_LUN_DEFINE(_name, _disk, _read_only)                            \
  static struct pbl_usb_msc__lun_state pbl_usb_msc_lun_state_##_name;               \
  static const struct pbl_usb_msc_lun pbl_usb_msc_lun_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_usb_msc_lun, usb_msc_luns, #_name) = {                       \
    .disk = (_disk),                                                                \
    .read_only = (_read_only),                                                      \
    .state = &pbl_usb_msc_lun_state_##_name,                                        \
  }

/** @} */
