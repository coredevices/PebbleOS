/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/uart.h>

/**
 * @defgroup drivers_uart_posix POSIX
 * @ingroup drivers_uart
 * @brief @ref drivers_uart driver on the host: the terminal, a TCP port or a serial device.
 *
 * The port has no line settings: any baud rate is accepted.
 * @{
 */

/** @brief Host end of the console. */
#define PBL_UART_POSIX_CONSOLE 0
/** @brief Host end of the QEMU serial protocol. */
#define PBL_UART_POSIX_QEMU 1
/** @brief Host end of Bluetooth HCI. */
#define PBL_UART_POSIX_BT_HCI 2

/** @brief POSIX serial port. */
struct pbl_uart_posix {
  /** Serial port. */
  struct pbl_uart uart;
  /** Host end, one of @c PBL_UART_POSIX_*. */
  uint8_t channel;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_ops pbl_uart_posix_ops;
/** @endcond */

/**
 * @brief Define a POSIX serial port.
 *
 * @param sym Symbol of the port.
 * @param _name Name.
 * @param _channel Host end, one of @c PBL_UART_POSIX_*.
 * @param _baudrate Initial baud rate, in bits per second.
 */
#define PBL_UART_POSIX_DEFINE(sym, _name, _channel, _baudrate)                  \
  PBL_UART_STATE_DEFINE(sym);                                                   \
  const struct pbl_uart_posix sym = {                                           \
    .uart = PBL_UART_INIT(sym, _name, &pbl_uart_posix_ops, _baudrate, nullptr), \
    .channel = (_channel),                                                      \
  };                                                                            \
  PBL_DEVICE_REGISTER(sym, &sym.uart.dev)

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart_posix pbl_uart_posix_console;
extern const struct pbl_uart_posix pbl_uart_posix_qemu;
extern const struct pbl_uart_posix pbl_uart_posix_bt_hci;
/** @endcond */

/** @brief The POSIX console port. */
#define POSIX_CONSOLE_UART (&pbl_uart_posix_console.uart)
/** @brief The POSIX QEMU serial protocol port. */
#define POSIX_QEMU_UART (&pbl_uart_posix_qemu.uart)
/** @brief The POSIX Bluetooth HCI port, with @c CONFIG_BT_HCI_UART. */
#define POSIX_BT_HCI_UART (&pbl_uart_posix_bt_hci.uart)

/** @} */
