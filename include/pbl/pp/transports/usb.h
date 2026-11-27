/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/pp/transport.h>

/**
 * @defgroup pp_transport_usb USB transport
 * @ingroup pp_transport
 * @brief Pebble protocol over USB, for development tools: a function of the USB device.
 *
 * An interface of its own class, with a bulk OUT and a bulk IN endpoint, which carry the Pebble
 * protocol messages as a byte stream each way. A transfer ends with a short packet, a zero-length
 * one if it fills its last packet. The host opens a session with the vendor request 0x01 to the
 * interface, which also ends the session open before, and closes it with 0x02. The application
 * connects the device to the bus with pbl_usbd_enable().
 * @{
 */

/** @brief Interface class, vendor-specific. */
#define PBL_PP_USB_CLASS 0xffU

/** @brief Interface subclass, which tells the interface from other vendor-specific ones. */
#define PBL_PP_USB_SUBCLASS 0x50U

/** @brief Interface protocol. */
#define PBL_PP_USB_PROTOCOL 0x01U

/** @brief The transport, to tell its sessions from others, see pbl_pp_session_get_transport(). */
extern const struct pbl_pp_transport pbl_pp_usb_transport;

/** @} */
