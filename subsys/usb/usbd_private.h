/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/usb/usb.h>
#include <pbl/usb/usbd.h>

// First interface of the device a function's interface 0 is, and how many it has
uint8_t pbl_usbd__first_iface(const struct pbl_usbd_function *fn);
uint8_t pbl_usbd__num_ifaces(const struct pbl_usbd_function *fn);

// BOS descriptor into out: its length, or a negative errno
int pbl_usbd__bos_desc(uint8_t *out, uint16_t room);

// Answer to an IN vendor request to the device into out: its length, or a negative errno
int pbl_usbd__bos_vendor(const struct pbl_usb_setup *setup, uint8_t *out, uint16_t room);
