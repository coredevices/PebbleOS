/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/timer.h>

/**
 * @defgroup drivers_mbox Mailbox
 * @ingroup drivers
 * @brief Mailbox controller class.
 *
 * A mailbox controller passes messages, or just doorbells, between processors over channels. A
 * consumer holds a struct pbl_mbox_chan, what a devicetree @c mboxes entry describes: the
 * controller and its specifier cells, which the controller translates to one of its channels. It
 * requests the channel with a struct pbl_mbox_client, usually embedded in its own state so the
 * callbacks get back to it with container_of():
 *
 * @code{.c}
 * static void prv_rx(const struct pbl_mbox_client *cl, const void *msg) {
 *   const struct foo *foo = container_of(cl, const struct foo, cl);
 *   // interrupt context
 * }
 *
 * ret = pbl_mbox_request(&foo->rx_chan, &foo->cl);
 * ret = pbl_mbox_request(&foo->tx_chan, &foo->cl);
 * ret = pbl_mbox_send(&foo->tx_chan, nullptr);
 * @endcode
 *
 * Doorbell controllers carry no data: messages are @c nullptr and the payload lives in shared
 * memory. One message per channel is in flight; sending while one is in flight queues the new
 * message, replacing an already queued one, and the controller sends it once the first is done.
 * The class reports completed messages through the client's @c tx_done, polling the controller
 * when it has no completion interrupt.
 * @{
 */

struct pbl_mbox_controller;

/** @brief Class of the mailbox controllers. */
extern const struct pbl_device_class pbl_mbox_class;

/** @brief Maximum number of specifier cells. */
#define PBL_MBOX_MAX_CELLS 2U

/** @brief A channel, as used by a consumer. */
struct pbl_mbox_chan {
  /** Controller. */
  const struct pbl_mbox_controller *ctrl;
  /** Specifier cells, as interpreted by the controller. */
  uint32_t cells[PBL_MBOX_MAX_CELLS];
};

/**
 * @brief Initializer of a struct pbl_mbox_chan.
 *
 * @param _ctrl Controller.
 * @param ... Specifier cells.
 */
#define PBL_MBOX_CHAN(_ctrl, ...)             \
  {                                           \
    .ctrl = (_ctrl), .cells = { __VA_ARGS__ } \
  }

/** @brief Channel client. */
struct pbl_mbox_client {
  /**
   * Message received, from interrupt context.
   *
   * @param cl Client.
   * @param msg Message, @c nullptr for doorbells.
   */
  void (*rx_callback)(const struct pbl_mbox_client *cl, const void *msg);
  /**
   * Message sent, from interrupt context.
   *
   * @param cl Client.
   * @param msg Message given to pbl_mbox_send().
   * @param ret 0, or a negative errno.
   */
  void (*tx_done)(const struct pbl_mbox_client *cl, const void *msg, int ret);
};

/** @brief Channel state, kept by the class. */
struct pbl_mbox_chan_state {
  /** Client, set while the channel is requested. */
  const struct pbl_mbox_client *cl;
  /** Message in flight. */
  const void *tx_msg;
  /** Message queued behind @ref tx_msg. */
  const void *tx_next;
  /** Whether @ref tx_msg is in flight. */
  bool tx_active;
  /** Whether @ref tx_next is queued. */
  bool tx_queued;
  /** Messages sent. */
  uint32_t tx_count;
  /** Messages received. */
  uint32_t rx_count;
};

/** @brief Controller state, kept by the class. */
struct pbl_mbox_controller_state {
  /** Completion poll timer. */
  struct pbl_timer poll;
  /** Controller. */
  const struct pbl_mbox_controller *ctrl;
};

/** @brief Controller operations, implemented by the providers. */
struct pbl_mbox_controller_ops {
  /** Translate a consumer's specifier to a channel index, or return a negative errno. */
  int (*xlate)(const struct pbl_mbox_controller *ctrl, const struct pbl_mbox_chan *chan);
  /** Enable a channel. */
  int (*startup)(const struct pbl_mbox_controller *ctrl, uint8_t idx);
  /** Disable a channel. */
  void (*shutdown)(const struct pbl_mbox_controller *ctrl, uint8_t idx);
  /** Send a message on a channel. */
  int (*send_data)(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg);
  /** Whether the remote took the last message on a channel, for polled completion. */
  bool (*last_tx_done)(const struct pbl_mbox_controller *ctrl, uint8_t idx);
};

/** @brief Mailbox controller. */
struct pbl_mbox_controller {
  /** Device. */
  struct pbl_device dev;
  /** Operations. */
  const struct pbl_mbox_controller_ops *ops;
  /** Controller state. */
  struct pbl_mbox_controller_state *state;
  /** Channel states, @ref num_chans entries. */
  struct pbl_mbox_chan_state *chans;
  /** Number of channels. */
  uint8_t num_chans;
  /** Completion poll period in milliseconds; 0 if the provider reports completion itself. */
  uint16_t txpoll_period_ms;
};

/**
 * @brief Define the state of a controller, for providers.
 *
 * @param sym Controller symbol.
 * @param _num_chans Number of channels.
 */
#define PBL_MBOX_CONTROLLER_STATE_DEFINE(sym, _num_chans)        \
  PBL_DEVICE_STATE_DEFINE(sym);                                  \
  static struct pbl_mbox_controller_state sym##_mbox_ctrl_state; \
  static struct pbl_mbox_chan_state sym##_mbox_chans[_num_chans]

/**
 * @brief Initializer of a struct pbl_mbox_controller, for providers.
 *
 * @param sym Controller symbol, with its PBL_MBOX_CONTROLLER_STATE_DEFINE().
 * @param _name Device name.
 * @param _init Device init function.
 * @param _deps Device dependencies.
 * @param _ops Operations.
 * @param _txpoll_period_ms Completion poll period in milliseconds, 0 for none.
 */
#define PBL_MBOX_CONTROLLER_INIT(sym, _name, _init, _deps, _ops, _txpoll_period_ms)   \
  {                                                                                   \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, _deps, &pbl_mbox_class), \
    .ops = (_ops),                                                                    \
    .state = &sym##_mbox_ctrl_state,                                                  \
    .chans = sym##_mbox_chans,                                                        \
    .num_chans = sizeof(sym##_mbox_chans) / sizeof(sym##_mbox_chans[0]),              \
    .txpoll_period_ms = (_txpoll_period_ms),                                          \
  }

/**
 * @brief Request a channel and enable it.
 *
 * @param chan Channel.
 * @param cl Client, must outlive the request.
 * @return 0 on success, -ENODEV if the controller is not ready, -EINVAL for a bad specifier,
 *         -EBUSY if the channel is already requested, or a negative errno from the provider.
 */
int pbl_mbox_request(const struct pbl_mbox_chan *chan, const struct pbl_mbox_client *cl);

/**
 * @brief Disable a channel and release it, dropping any message in flight.
 *
 * @param chan Channel, requested.
 */
void pbl_mbox_free(const struct pbl_mbox_chan *chan);

/**
 * @brief Send a message.
 *
 * Does not wait: the client's @c tx_done reports when the remote took it.
 *
 * @param chan Channel, requested.
 * @param msg Message, @c nullptr for doorbells.
 * @return 0 on success, -EINVAL if the channel is not requested, or a negative errno from the
 *         provider.
 */
int pbl_mbox_send(const struct pbl_mbox_chan *chan, const void *msg);

/**
 * @brief Report a message from the remote, for providers.
 *
 * Called from interrupt context.
 *
 * @param ctrl Controller.
 * @param idx Channel index.
 * @param msg Message, @c nullptr for doorbells.
 */
void pbl_mbox_chan_received(const struct pbl_mbox_controller *ctrl, uint8_t idx, const void *msg);

/**
 * @brief Report that the remote took the last message, for providers with a completion interrupt.
 *
 * Called from interrupt context.
 *
 * @param ctrl Controller.
 * @param idx Channel index.
 * @param ret 0, or a negative errno.
 */
void pbl_mbox_chan_txdone(const struct pbl_mbox_controller *ctrl, uint8_t idx, int ret);

/** @} */
