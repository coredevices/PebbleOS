/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/uart.h>

/**
 * @defgroup usb_cdc_acm USB serial port
 * @ingroup usbd
 * @brief A serial port over USB, CDC ACM, as a UART: a board's roles can give it to the console.
 *
 * It takes interfaces 0 and 1, and endpoints 0x81, 0x02 and 0x82. The device connects to the bus
 * when the port comes up. Bytes sent before the host configures the device are dropped, as are
 * those that do not fit while the host does not read.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart pbl_usb_cdc_acm;
/** @endcond */

/** The serial port over USB, for a role in a board's or an application's roles.yml. */
#define USB_CDC_ACM_UART (&pbl_usb_cdc_acm)

/** @} */
