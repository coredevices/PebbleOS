/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/uart.h>
#include <pbl/util/listify.h>

/**
 * @defgroup usb_cdc_acm USB serial port
 * @ingroup usbd
 * @brief Serial ports over USB, CDC ACM, as UARTs: a board's roles can give one to the console.
 *
 * There are @c CONFIG_USB_CDC_ACM_COUNT ports, in one function of the device. Each port is an
 * interface association of two interfaces, with three endpoints, which the stack numbers. The
 * host shows @c CONFIG_USB_CDC_ACM_NAME as the name of a single port, followed by @c n for port
 * @c n of several. The device connects to the bus when a port comes up. Bytes sent before the
 * host configures the device are dropped, as are those that do not fit while the host does not
 * read.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define PBL_USB_CDC_ACM_DECLARE(n) extern const struct pbl_uart pbl_usb_cdc_acm_##n;
PBL_LISTIFY(CONFIG_USB_CDC_ACM_COUNT, PBL_USB_CDC_ACM_DECLARE)
/** @endcond */

/**
 * @brief Serial port @p n over USB, for a role in a board's or an application's roles.yml.
 *
 * @param n Port, a decimal literal below @c CONFIG_USB_CDC_ACM_COUNT.
 */
#define USB_CDC_ACM_UART(n) (&pbl_usb_cdc_acm_##n)

/** @} */
