/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_uart UART
 * @ingroup drivers
 * @brief Serial port class.
 *
 * A serial port is a struct pbl_uart device. It starts with the baud rate the board gives, 8 data
 * bits, no parity, one stop bit and no flow control. Bytes are sent polled, and received bytes
 * are handed to a callback, one at a time, from the port's interrupt while reception is enabled:
 *
 * @code{.c}
 * static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
 *   // interrupt context; may call pbl_uart_rx_disable() to hold further bytes back
 * }
 *
 * const struct pbl_uart_config cfg = {.baudrate = 115200};
 *
 * ret = pbl_uart_configure(DBG_UART, &cfg);
 * ret = pbl_uart_rx_enable(DBG_UART, prv_rx, nullptr);
 *
 * pbl_uart_poll_out(DBG_UART, 'A');
 * pbl_uart_flush(DBG_UART);
 * @endcode
 *
 * The port may keep the SoC out of its deepest sleep states while reception is enabled. Calls
 * may be made from interrupt context. They return 0 or a negative errno: @c -ENODEV if the port
 * is not ready and @c -ENOTSUP for line settings it does not support.
 * @{
 */

/** @brief Parity. */
enum pbl_uart_parity {
  /** No parity bit. */
  PBL_UART_PARITY_NONE,
  /** Odd parity. */
  PBL_UART_PARITY_ODD,
  /** Even parity. */
  PBL_UART_PARITY_EVEN,
};

/** @brief Flow control. */
enum pbl_uart_flow_ctrl {
  /** None. */
  PBL_UART_FLOW_CTRL_NONE,
  /** RTS and CTS lines. */
  PBL_UART_FLOW_CTRL_RTS_CTS,
};

/** @brief Line settings. */
struct pbl_uart_config {
  /** Baud rate, in bits per second. */
  uint32_t baudrate;
  /** Parity. */
  enum pbl_uart_parity parity;
  /** Flow control. */
  enum pbl_uart_flow_ctrl flow_ctrl;
};

struct pbl_uart;

/**
 * @brief Receive callback, called from the port's interrupt for each received byte.
 *
 * @param uart Port.
 * @param c Byte.
 * @param arg Callback argument.
 */
typedef void (*pbl_uart_rx_callback_t)(const struct pbl_uart *uart, uint8_t c, void *arg);

/** @brief Class of the serial ports. */
extern const struct pbl_device_class pbl_uart_class;

/** @cond INTERNAL_HIDDEN */
struct pbl_uart_state {
  struct pbl_uart_config cfg;
  pbl_uart_rx_callback_t rx_cb;
  void *rx_arg;
  bool rx_enabled;
};
/** @endcond */

/**
 * @brief Port driver operations.
 *
 * Called with interrupts locked, except init().
 */
struct pbl_uart_ops {
  /** Set the port up for transmission. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_uart *uart);
  /** Apply line settings. Returns 0 or a negative errno. */
  int (*configure)(const struct pbl_uart *uart, const struct pbl_uart_config *cfg);
  /** Send a byte, waiting for room. */
  void (*poll_out)(const struct pbl_uart *uart, uint8_t c);
  /** Optional. Wait until the bytes sent have left the port. */
  void (*flush)(const struct pbl_uart *uart);
  /**
   * Start reception, handing bytes to pbl_uart_receive(). Returns 0 or a negative errno. May be
   * called from the port's interrupt.
   */
  int (*rx_enable)(const struct pbl_uart *uart);
  /** Stop reception. May be called from the port's interrupt. */
  void (*rx_disable)(const struct pbl_uart *uart);
};

/** @brief A serial port. */
struct pbl_uart {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_uart_ops *ops;
  /** Runtime state. */
  struct pbl_uart_state *state;
  /** Initial baud rate, in bits per second. */
  uint32_t baudrate;
};

/**
 * @brief Define the class state of port @p sym, for the @c PBL_UART_*_DEFINE() macro of a driver.
 *
 * @param sym Symbol of the port instance.
 */
#define PBL_UART_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);    \
  static struct pbl_uart_state sym##_uart_state

/**
 * @brief Initializer for the struct pbl_uart of port @p sym.
 *
 * @param sym Symbol of the port instance, with its state defined by PBL_UART_STATE_DEFINE().
 * @param _name Name.
 * @param _ops Driver operations.
 * @param _baudrate Initial baud rate, in bits per second.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 */
#define PBL_UART_INIT(sym, _name, _ops, _baudrate, _deps)                                     \
  {                                                                                           \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_uart_init, nullptr, _deps, &pbl_uart_class), \
    .ops = (_ops),                                                                            \
    .state = &sym##_uart_state,                                                               \
    .baudrate = (_baudrate),                                                                  \
  }

/**
 * @brief Device init of every port: calls the driver's init, then applies the line settings.
 *
 * @param dev Port device.
 * @return 0 or a negative errno.
 */
int pbl_uart_init(const struct pbl_device *dev);

/**
 * @brief Hand a received byte to the receive callback.
 *
 * Called by the port driver from its interrupt.
 *
 * @param uart Port.
 * @param c Byte.
 * @return true if reception is still enabled, false if the driver must hold further bytes back.
 */
bool pbl_uart_receive(const struct pbl_uart *uart, uint8_t c);

/**
 * @brief Apply line settings.
 *
 * @param uart Port.
 * @param cfg Line settings.
 * @return 0 or a negative errno.
 */
int pbl_uart_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg);

/**
 * @brief Get the line settings.
 *
 * @param uart Port.
 * @param[out] cfg Line settings.
 * @return 0 or a negative errno.
 */
int pbl_uart_config_get(const struct pbl_uart *uart, struct pbl_uart_config *cfg);

/**
 * @brief Send a byte, waiting for room. Does nothing if the port is not ready.
 *
 * @param uart Port.
 * @param c Byte.
 */
void pbl_uart_poll_out(const struct pbl_uart *uart, uint8_t c);

/**
 * @brief Wait until the bytes sent have left the port.
 *
 * @param uart Port.
 */
void pbl_uart_flush(const struct pbl_uart *uart);

/**
 * @brief Enable reception.
 *
 * Bytes are handed to @p cb until pbl_uart_rx_disable() is called. Enabling it again replaces
 * the callback.
 *
 * @param uart Port.
 * @param cb Receive callback.
 * @param arg Callback argument.
 * @return 0 or a negative errno.
 */
int pbl_uart_rx_enable(const struct pbl_uart *uart, pbl_uart_rx_callback_t cb, void *arg);

/**
 * @brief Disable reception. Bytes the port does not hold back are lost.
 *
 * @param uart Port.
 * @return 0 or a negative errno.
 */
int pbl_uart_rx_disable(const struct pbl_uart *uart);

/** @} */
