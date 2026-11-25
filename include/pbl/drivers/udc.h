/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_udc USB device controller
 * @ingroup drivers
 * @brief The device side of a USB controller, which the USB device stack drives.
 *
 * Endpoints are named by their address: the number, with @ref PBL_UDC_EP_IN for the IN direction.
 * Endpoint 0 is set up by the driver; the stack runs control transfers on it as a pair, 0x00 and
 * 0x80.
 *
 * Transfers are queued one at a time per endpoint with pbl_udc_ep_queue(). An IN transfer sends
 * its bytes, a zero-length packet when it has none; an OUT transfer ends with a short packet or
 * once its buffer is full. The driver reports the end of each with pbl_udc_xfer_done(), and bus
 * events, setup packets among them, with pbl_udc_event(), both from its interrupt: the stack's
 * callbacks run there.
 *
 * The driver takes the address as soon as the stack sets it, before the status stage of the
 * SET_ADDRESS request, as controllers that apply it in hardware need.
 * @{
 */

/** Direction bit of an IN endpoint address. */
#define PBL_UDC_EP_IN 0x80U

/**
 * Capability of a controller whose endpoint numbers each serve one direction: endpoint n is IN
 * or OUT, not both.
 */
#define PBL_UDC_CAP_EP_ONE_DIR 0x01U

/** @brief Endpoint transfer types, as the endpoint descriptor gives them. */
enum pbl_udc_ep_type {
  /** Control. */
  PBL_UDC_EP_CONTROL = 0,
  /** Isochronous. */
  PBL_UDC_EP_ISOCHRONOUS = 1,
  /** Bulk. */
  PBL_UDC_EP_BULK = 2,
  /** Interrupt. */
  PBL_UDC_EP_INTERRUPT = 3,
};

/** @brief Bus speeds. */
enum pbl_udc_speed {
  /** Full speed, 12 Mb/s. */
  PBL_UDC_SPEED_FULL,
  /** High speed, 480 Mb/s. */
  PBL_UDC_SPEED_HIGH,
};

/** @brief Bus events. */
enum pbl_udc_event {
  /** The host reset the bus: the device is back to its default state, at address 0. */
  PBL_UDC_EVENT_RESET,
  /** The bus speed is known, after a reset. */
  PBL_UDC_EVENT_ENUMERATED,
  /** The bus went idle. */
  PBL_UDC_EVENT_SUSPEND,
  /** The bus is active again. */
  PBL_UDC_EVENT_RESUME,
  /** A setup packet arrived on endpoint 0. */
  PBL_UDC_EVENT_SETUP,
};

struct pbl_udc;

/** @brief What the stack does with what a controller reports, from its interrupt. */
struct pbl_udc_callbacks {
  /** A bus event; for @ref PBL_UDC_EVENT_SETUP, @p setup holds the 8 bytes of the packet. */
  void (*event)(const struct pbl_udc *udc, enum pbl_udc_event evt, const uint8_t *setup, void *arg);
  /** The transfer queued on @p ep ended: @p ret bytes moved, or a negative errno. */
  void (*xfer_done)(const struct pbl_udc *udc, uint8_t ep, int ret, void *arg);
};

/** @cond INTERNAL_HIDDEN */
struct pbl_udc_state {
  const struct pbl_udc_callbacks *cb;
  void *arg;
};
/** @endcond */

/** @brief Driver operations. */
struct pbl_udc_ops {
  /** Bring the controller up, disconnected. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_udc *udc);
  /** Connect to the bus. Returns 0 or a negative errno. */
  int (*enable)(const struct pbl_udc *udc);
  /** Disconnect from the bus. Returns 0 or a negative errno. */
  int (*disable)(const struct pbl_udc *udc);
  /** The speed the bus runs at, once enumerated. */
  enum pbl_udc_speed (*speed)(const struct pbl_udc *udc);
  /** Take the address the host gave. Returns 0 or a negative errno. */
  int (*set_address)(const struct pbl_udc *udc, uint8_t addr);
  /** Set an endpoint up. Returns 0 or a negative errno. */
  int (*ep_enable)(const struct pbl_udc *udc, uint8_t ep, enum pbl_udc_ep_type type, uint16_t mps);
  /** Shut an endpoint down, ending its transfer with @c -ECONNABORTED. */
  int (*ep_disable)(const struct pbl_udc *udc, uint8_t ep);
  /** Stall an endpoint, or clear its stall and its data toggle. */
  int (*ep_set_halt)(const struct pbl_udc *udc, uint8_t ep, bool halt);
  /** Start a transfer. Returns 0, @c -EBUSY if one is under way, or another negative errno. */
  int (*ep_queue)(const struct pbl_udc *udc, uint8_t ep, void *buf, size_t len);
};

/** @brief USB device controller. */
struct pbl_udc {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_udc_ops *ops;
  /** Runtime state. */
  struct pbl_udc_state *state;
  /** Number of endpoints in each direction, endpoint 0 included. */
  uint8_t num_eps;
  /** Fastest speed the controller and its PHY run at. */
  enum pbl_udc_speed max_speed;
  /** Capabilities, as @ref PBL_UDC_CAP_EP_ONE_DIR. */
  uint8_t caps;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_udc_class;

int pbl_udc_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The device controller of the board, for the USB device stack. */
extern const struct pbl_udc *const UDC;

/**
 * @brief Define the runtime state of a controller.
 *
 * @param sym Symbol of the controller.
 */
#define PBL_UDC_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_udc_state sym##_udc_state

/**
 * @brief Initializer of a controller.
 *
 * @param sym Symbol of the controller.
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _num_eps Number of endpoints in each direction.
 * @param _max_speed Fastest speed, one of @ref pbl_udc_speed.
 * @param _caps Capabilities, as @ref PBL_UDC_CAP_EP_ONE_DIR, or 0.
 */
#define PBL_UDC_INIT(sym, _name, _parent, _ops, _num_eps, _max_speed, _caps)             \
  {                                                                                      \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_udc_dev_init, _parent, &pbl_udc_class), \
    .ops = (_ops),                                                                       \
    .state = &sym##_udc_state,                                                           \
    .num_eps = (_num_eps),                                                               \
    .max_speed = (_max_speed),                                                           \
    .caps = (_caps),                                                                     \
  }

/**
 * @brief Take what the controller reports.
 *
 * @param udc Controller.
 * @param cb Callbacks, which run from the controller's interrupt.
 * @param arg Argument of the callbacks.
 */
void pbl_udc_register(const struct pbl_udc *udc, const struct pbl_udc_callbacks *cb, void *arg);

/**
 * @brief Connect to the bus.
 *
 * @param udc Controller.
 * @return 0, @c -ENODEV if the controller is not ready, or another negative errno.
 */
int pbl_udc_enable(const struct pbl_udc *udc);

/**
 * @brief Disconnect from the bus.
 *
 * @param udc Controller.
 * @return 0 or a negative errno.
 */
int pbl_udc_disable(const struct pbl_udc *udc);

/**
 * @brief The speed the bus runs at, once enumerated.
 *
 * @param udc Controller.
 * @return Speed.
 */
enum pbl_udc_speed pbl_udc_speed(const struct pbl_udc *udc);

/**
 * @brief Take the address the host gave, before the status stage of its request.
 *
 * @param udc Controller.
 * @param addr Address.
 * @return 0 or a negative errno.
 */
int pbl_udc_set_address(const struct pbl_udc *udc, uint8_t addr);

/**
 * @brief Set an endpoint up.
 *
 * @param udc Controller.
 * @param ep Endpoint address.
 * @param type Transfer type.
 * @param mps Maximum packet size.
 * @return 0, @c -EINVAL for an endpoint the controller lacks, or another negative errno.
 */
int pbl_udc_ep_enable(const struct pbl_udc *udc, uint8_t ep, enum pbl_udc_ep_type type,
                      uint16_t mps);

/**
 * @brief Shut an endpoint down.
 *
 * @param udc Controller.
 * @param ep Endpoint address.
 * @return 0 or a negative errno.
 */
int pbl_udc_ep_disable(const struct pbl_udc *udc, uint8_t ep);

/**
 * @brief Stall an endpoint, or clear its stall.
 *
 * @param udc Controller.
 * @param ep Endpoint address.
 * @param halt Whether to stall it.
 * @return 0 or a negative errno.
 */
int pbl_udc_ep_set_halt(const struct pbl_udc *udc, uint8_t ep, bool halt);

/**
 * @brief Start a transfer.
 *
 * @param udc Controller.
 * @param ep Endpoint address.
 * @param buf Data to send, or room for what arrives, until the transfer ends.
 * @param len Its length, in bytes.
 * @return 0, @c -EBUSY if a transfer is under way on the endpoint, or another negative errno.
 */
int pbl_udc_ep_queue(const struct pbl_udc *udc, uint8_t ep, void *buf, size_t len);

/**
 * @brief Report a bus event, from the controller's interrupt.
 *
 * @param udc Controller.
 * @param evt Event.
 * @param setup For @ref PBL_UDC_EVENT_SETUP, the 8 bytes of the setup packet.
 */
void pbl_udc_event(const struct pbl_udc *udc, enum pbl_udc_event evt, const uint8_t *setup);

/**
 * @brief Report the end of a transfer, from the controller's interrupt.
 *
 * @param udc Controller.
 * @param ep Endpoint address.
 * @param ret Bytes moved, or a negative errno.
 */
void pbl_udc_xfer_done(const struct pbl_udc *udc, uint8_t ep, int ret);

/** @} */
