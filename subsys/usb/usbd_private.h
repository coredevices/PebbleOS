/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/usb/usb.h>

// BOS descriptor into out: its length, or a negative errno
int pbl_usbd__bos_desc(uint8_t *out, uint16_t room);

// Answer to an IN vendor request to the device into out: its length, or a negative errno
int pbl_usbd__bos_vendor(const struct pbl_usb_setup *setup, uint8_t *out, uint16_t room);
