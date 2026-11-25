/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash/partition.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup usb_dfu USB firmware upgrade
 * @ingroup usbd
 * @brief Device Firmware Upgrade 1.1, in DFU mode: a host downloads an image to a flash partition
 * and uploads it back, as with dfu-util.
 *
 * The function has one interface, numbered by the stack among the other functions', and uses only
 * endpoint 0. The partition it writes is a region the application defines with
 * PBL_USB_DFU_REGION_DEFINE(): the function erases its subsectors as the download reaches them,
 * and never writes outside it. A download starts at the beginning of the region, an upload reads
 * it whole, ending with a short block. The flash work runs on the system work queue, while the
 * host polls the status.
 *
 * @code{.c}
 * PBL_USB_DFU_REGION_DEFINE(image, PART_DFU);
 *
 * ret = pbl_usbd_enable();
 * @endcode
 * @{
 */

/** @brief A flash partition the host can download to and upload from. */
struct pbl_usb_dfu_region {
  /** Partition. */
  struct pbl_flash_partition part;
};

/**
 * @brief Define a region of the function.
 *
 * Only one region is supported, alternate setting 0 of the interface.
 *
 * @param _name Name of the region.
 * @param _part Partition, as the prefix of its @c _OFFSET and @c _SIZE constants, e.g. @c PART_DFU.
 */
#define PBL_USB_DFU_REGION_DEFINE(_name, _part)                                           \
  static const struct pbl_usb_dfu_region pbl_usb_dfu_region_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_usb_dfu_region, usb_dfu_regions, #_name) = {                       \
    .part = {.offset = _part##_OFFSET, .size = _part##_SIZE},                             \
  }

/** @} */
