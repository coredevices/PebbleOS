/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/types.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_can CAN
 * @ingroup drivers
 * @brief CAN controller class, for CAN and CAN FD.
 *
 * A CAN controller is a struct pbl_can device. It starts stopped: it is given its bit rates and
 * mode with pbl_can_configure(), and its acceptance filters with pbl_can_add_filter(), while
 * stopped, then joins the bus with pbl_can_start(). Received frames are handed to a callback
 * from the controller's interrupt, and frames are sent with pbl_can_send(), either waiting for
 * the transmission or with a callback for its outcome:
 *
 * @code{.c}
 * static void prv_rx(const struct pbl_can *can, const struct pbl_can_frame *frame, void *arg) {
 *   // interrupt context
 * }
 *
 * const struct pbl_can_config cfg = {.bitrate = 500000U};
 * const struct pbl_can_frame frame = {.id = 0x123, .len = 2, .data = {0xbe, 0xef}};
 *
 * ret = pbl_can_configure(can, &cfg);
 * pbl_can_set_rx_callback(can, prv_rx, nullptr);
 * ret = pbl_can_start(can);
 * ret = pbl_can_send(can, &frame, PBL_MSEC(100), nullptr, nullptr);
 * @endcode
 *
 * The class computes the bit timing from the bit rates, sample points and the controller's
 * clock, within the limits the driver gives, and only accepts bit rates the clock divides
 * exactly. Without filters, the controller accepts every frame.
 *
 * Calls return 0 or a negative errno: @c -ENODEV if the controller is not ready, @c -ENOTSUP for
 * a mode it does not support, @c -EBUSY for a call that needs it stopped and @c -ENETDOWN for one
 * that needs it started.
 * @{
 */

/** @brief Largest standard (11-bit) identifier. */
#define PBL_CAN_STD_ID_MASK 0x7ffU

/** @brief Largest extended (29-bit) identifier. */
#define PBL_CAN_EXT_ID_MASK 0x1fffffffU

/** @brief Most data bytes of a CAN frame. */
#define PBL_CAN_MAX_LEN 8U

/** @brief Most data bytes of a CAN FD frame. */
#define PBL_CAN_FD_MAX_LEN 64U

/**
 * @name Frame flags
 * @{
 */
/** Extended (29-bit) identifier. */
#define PBL_CAN_FRAME_IDE PBL_BIT(0)
/** Remote frame, CAN only. */
#define PBL_CAN_FRAME_RTR PBL_BIT(1)
/** CAN FD frame. */
#define PBL_CAN_FRAME_FDF PBL_BIT(2)
/** CAN FD frame whose data phase goes at the data bit rate. */
#define PBL_CAN_FRAME_BRS PBL_BIT(3)
/** CAN FD frame from an error passive node, on reception. */
#define PBL_CAN_FRAME_ESI PBL_BIT(4)
/** @} */

/**
 * @name Modes
 * @{
 */
/** Receive the frames sent, without driving the bus. */
#define PBL_CAN_MODE_LOOPBACK PBL_BIT(0)
/** Receive only, without acknowledging frames or signalling errors; sending is refused. */
#define PBL_CAN_MODE_LISTEN_ONLY PBL_BIT(1)
/** Send and receive CAN FD frames, as ISO 11898-1:2015 specifies them. */
#define PBL_CAN_MODE_FD PBL_BIT(2)
/** With @ref PBL_CAN_MODE_FD, CAN FD frames as first specified, without the stuff count. */
#define PBL_CAN_MODE_FD_NON_ISO PBL_BIT(3)
/** Send each frame once, without retransmitting it after an error or a lost arbitration. */
#define PBL_CAN_MODE_ONE_SHOT PBL_BIT(4)
/** Stay bus off until pbl_can_recover(), instead of recovering on its own. */
#define PBL_CAN_MODE_MANUAL_RECOVERY PBL_BIT(5)
/** @} */

/** @brief A CAN or CAN FD frame. */
struct pbl_can_frame {
  /** Identifier, standard or extended as @ref PBL_CAN_FRAME_IDE says. */
  uint32_t id;
  /** Frame flags, @c PBL_CAN_FRAME_*. */
  uint8_t flags;
  /** Data bytes: up to 8, or for CAN FD 12, 16, 20, 24, 32, 48 or 64. */
  uint8_t len;
  /** Data. */
  union {
    uint8_t data[PBL_CAN_FD_MAX_LEN];
    uint32_t data32[PBL_CAN_FD_MAX_LEN / 4U];
  };
};

/** @brief Bus configuration. */
struct pbl_can_config {
  /** Nominal bit rate, in bits per second. */
  uint32_t bitrate;
  /** Nominal sample point, in tenths of a percent, or 0 for the class default. */
  uint16_t sample_point;
  /** Data phase sample point, in tenths of a percent, or 0 for the class default. */
  uint16_t data_sample_point;
  /** Data phase bit rate of CAN FD frames, in bits per second, with @ref PBL_CAN_MODE_FD. */
  uint32_t data_bitrate;
  /** Mode, @c PBL_CAN_MODE_* flags. */
  uint32_t mode;
};

/** @brief Acceptance filter: frames whose identifier matches @p id in the bits of @p mask. */
struct pbl_can_filter {
  /** Identifier. */
  uint32_t id;
  /** Bits of the identifier that have to match. */
  uint32_t mask;
  /** Frame flags: @ref PBL_CAN_FRAME_IDE for extended identifiers, or none for standard ones. */
  uint8_t flags;
};

/** @brief Bit timing of one phase, in time quanta. */
struct pbl_can_timing {
  /** Clock periods per time quantum. */
  uint16_t prescaler;
  /** Propagation and phase segment 1. */
  uint16_t tseg1;
  /** Phase segment 2. */
  uint16_t tseg2;
  /** Synchronization jump width. */
  uint16_t sjw;
};

/** @brief Bit timing limits of one phase. */
struct pbl_can_timing_limits {
  /** Shortest propagation and phase segment 1. */
  uint16_t tseg1_min;
  /** Longest propagation and phase segment 1. */
  uint16_t tseg1_max;
  /** Shortest phase segment 2. */
  uint16_t tseg2_min;
  /** Longest phase segment 2. */
  uint16_t tseg2_max;
  /** Largest synchronization jump width. */
  uint16_t sjw_max;
  /** Smallest prescaler. */
  uint16_t prescaler_min;
  /** Largest prescaler. */
  uint16_t prescaler_max;
};

/** @brief Fault confinement state. */
enum pbl_can_bus_state {
  /** Stopped, off the bus. */
  PBL_CAN_STATE_STOPPED,
  /** Error active: both error counters below 96. */
  PBL_CAN_STATE_ERROR_ACTIVE,
  /** Error warning: an error counter at 96 or more. */
  PBL_CAN_STATE_ERROR_WARNING,
  /** Error passive: an error counter above 127. */
  PBL_CAN_STATE_ERROR_PASSIVE,
  /** Bus off: the transmit error counter went past 255. */
  PBL_CAN_STATE_BUS_OFF,
};

/** @brief Error counters. */
struct pbl_can_err_cnt {
  /** Transmit error counter. */
  uint8_t tx;
  /** Receive error counter. */
  uint8_t rx;
};

/** @brief Kinds of errors the controller counts. */
enum pbl_can_error {
  /** Bit error: the node sent a bit and read back another. */
  PBL_CAN_ERROR_BIT,
  /** Stuff error: six equal bits in a row. */
  PBL_CAN_ERROR_STUFF,
  /** Form error: a fixed-form field with a wrong bit. */
  PBL_CAN_ERROR_FORM,
  /** Acknowledgment error: no node acknowledged a frame sent. */
  PBL_CAN_ERROR_ACK,
  /** CRC error. */
  PBL_CAN_ERROR_CRC,
  /** Arbitration lost to another node, not an error on the bus. */
  PBL_CAN_ERROR_ARBITRATION_LOST,
  /** Frames received with no room left for them, dropped. */
  PBL_CAN_ERROR_RX_OVERRUN,
  /** Number of kinds of errors. */
  PBL_CAN_ERROR_COUNT,
};

/** @brief Counts of errors and bus off events, since the controller was initialized. */
struct pbl_can_stats {
  /** Errors, by @ref pbl_can_error. */
  uint32_t errors[PBL_CAN_ERROR_COUNT];
  /** Times the controller went bus off. */
  uint32_t bus_off;
};

struct pbl_can;

/**
 * @brief Receive callback, called from the controller's interrupt for each frame accepted.
 *
 * @param can Controller.
 * @param frame Frame, valid during the call.
 * @param arg Callback argument.
 */
typedef void (*pbl_can_rx_callback_t)(const struct pbl_can *can, const struct pbl_can_frame *frame,
                                      void *arg);

/**
 * @brief Transmit callback, called from the controller's interrupt once a frame is done.
 *
 * @param can Controller.
 * @param status 0 if the frame was sent, @c -ECANCELED if it was aborted, @c -ENETDOWN if the
 * controller stopped or went bus off first, @c -EIO if it failed with
 * @ref PBL_CAN_MODE_ONE_SHOT.
 * @param arg Callback argument.
 */
typedef void (*pbl_can_tx_callback_t)(const struct pbl_can *can, int status, void *arg);

/**
 * @brief State callback, called from the controller's interrupt when its state changes.
 *
 * @param can Controller.
 * @param state New state.
 * @param cnt Error counters.
 * @param arg Callback argument.
 */
typedef void (*pbl_can_state_callback_t)(const struct pbl_can *can, enum pbl_can_bus_state state,
                                         struct pbl_can_err_cnt cnt, void *arg);

/** @brief Class of the CAN controllers. */
extern const struct pbl_device_class pbl_can_class;

/** @brief The controller of the board's CAN bus connector. */
extern const struct pbl_can *const CAN_BUS;

/** @cond INTERNAL_HIDDEN */
struct pbl_can_state {
  struct pbl_mutex lock;
  struct pbl_sem tx_slots;
  struct pbl_can_config cfg;
  pbl_can_rx_callback_t rx_cb;
  void *rx_arg;
  pbl_can_state_callback_t state_cb;
  void *state_arg;
  uint32_t filters;
  struct pbl_can_stats stats;
  enum pbl_can_bus_state bus_state;
  bool configured;
  bool started;
};
/** @endcond */

/**
 * @brief Controller driver operations.
 *
 * init() is called from the device init. configure(), set_filter() and start() are called from a
 * thread with the device lock held, stop() also with interrupts locked. send(), abort() and
 * get_state() and recover() are called with interrupts locked.
 */
struct pbl_can_ops {
  /** Bring the controller up, stopped. Returns 0 or a negative errno. */
  int (*init)(const struct pbl_can *can);
  /** Get the rate of the clock time quanta are counted in, in Hz. Returns 0 or a negative errno. */
  int (*get_core_clock)(const struct pbl_can *can, uint32_t *rate);
  /**
   * Apply the bit timing, the data phase's NULL without @ref PBL_CAN_MODE_FD, and the mode, on
   * the next start. Returns 0 or a negative errno.
   */
  int (*configure)(const struct pbl_can *can, const struct pbl_can_timing *nominal,
                   const struct pbl_can_timing *data, uint32_t mode);
  /**
   * Set acceptance filter @p idx, or disable it if @p filter is NULL, on the next start. Returns 0
   * or a negative errno.
   */
  int (*set_filter)(const struct pbl_can *can, uint8_t idx, const struct pbl_can_filter *filter);
  /** Join the bus. Returns 0 or a negative errno. */
  int (*start)(const struct pbl_can *can);
  /** Leave the bus, completing the frames in flight with @c -ENETDOWN. */
  void (*stop)(const struct pbl_can *can);
  /**
   * Queue a frame, calling pbl_can_tx_done() with @p cb and @p arg once it is done. Returns 0, or
   * @c -EBUSY if the controller has no room for it.
   */
  int (*send)(const struct pbl_can *can, const struct pbl_can_frame *frame,
              pbl_can_tx_callback_t cb, void *arg);
  /** Abort the frames in flight, which complete with @c -ECANCELED unless they were sent. */
  void (*abort)(const struct pbl_can *can);
  /** Get the state and error counters, while started. Returns 0 or a negative errno. */
  int (*get_state)(const struct pbl_can *can, enum pbl_can_bus_state *state,
                   struct pbl_can_err_cnt *cnt);
  /**
   * Start the bus off recovery, with @ref PBL_CAN_MODE_MANUAL_RECOVERY. Returns 0 or a negative
   * errno.
   */
  int (*recover)(const struct pbl_can *can);
};

/** @brief A CAN controller. */
struct pbl_can {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_can_ops *ops;
  /** Runtime state. */
  struct pbl_can_state *state;
  /** Nominal bit timing limits. */
  const struct pbl_can_timing_limits *limits;
  /** Data phase bit timing limits, or NULL without CAN FD. */
  const struct pbl_can_timing_limits *data_limits;
  /** Supported modes, @c PBL_CAN_MODE_* flags. */
  uint32_t modes;
  /** Acceptance filters. */
  uint8_t num_filters;
};

/**
 * @brief Define the class state of controller @p sym, for the @c PBL_CAN_*_DEFINE() macro of a
 * driver.
 *
 * @param sym Symbol of the controller instance.
 * @param _tx_slots Frames the controller can have in flight.
 */
#define PBL_CAN_STATE_DEFINE(sym, _tx_slots)                   \
  PBL_DEVICE_STATE_DEFINE(sym);                                \
  static struct pbl_can_state sym##_can_state = {              \
    .lock = PBL_MUTEX_INITIALIZER,                             \
    .tx_slots = PBL_SEM_INITIALIZER((_tx_slots), (_tx_slots)), \
  }

/**
 * @brief Initializer for the struct pbl_can of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_CAN_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 * @param _limits Nominal bit timing limits.
 * @param _data_limits Data phase bit timing limits, or NULL without CAN FD.
 * @param _modes Supported modes.
 * @param _num_filters Acceptance filters.
 */
#define PBL_CAN_INIT(sym, _name, _parent, _ops, _limits, _data_limits, _modes, _num_filters) \
  {                                                                                          \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_can_dev_init, _parent, &pbl_can_class),     \
    .ops = (_ops),                                                                           \
    .state = &sym##_can_state,                                                               \
    .limits = (_limits),                                                                     \
    .data_limits = (_data_limits),                                                           \
    .modes = (_modes),                                                                       \
    .num_filters = (_num_filters),                                                           \
  }

/**
 * @brief Device init of every controller: calls the driver's init.
 *
 * @param dev Controller device.
 * @return 0 or a negative errno.
 */
int pbl_can_dev_init(const struct pbl_device *dev);

/**
 * @brief Hand a received frame to the receive callback.
 *
 * Called by the controller driver from its interrupt.
 *
 * @param can Controller.
 * @param frame Frame.
 */
void pbl_can_receive(const struct pbl_can *can, const struct pbl_can_frame *frame);

/**
 * @brief Complete a frame given to the driver's send().
 *
 * Called by the controller driver from its interrupt, or from its stop().
 *
 * @param can Controller.
 * @param cb Transmit callback given to send().
 * @param arg Callback argument given to send().
 * @param status 0, @c -ECANCELED or @c -ENETDOWN, as for @ref pbl_can_tx_callback_t.
 */
void pbl_can_tx_done(const struct pbl_can *can, pbl_can_tx_callback_t cb, void *arg, int status);

/**
 * @brief Report a state change.
 *
 * Called by the controller driver from its interrupt.
 *
 * @param can Controller.
 * @param state New state.
 * @param cnt Error counters.
 */
void pbl_can_report_state(const struct pbl_can *can, enum pbl_can_bus_state state,
                          struct pbl_can_err_cnt cnt);

/**
 * @brief Count an error.
 *
 * Called by the controller driver from its interrupt.
 *
 * @param can Controller.
 * @param error Kind of error.
 */
void pbl_can_report_error(const struct pbl_can *can, enum pbl_can_error error);

/**
 * @brief Compute the bit timing of one phase.
 *
 * Picks the smallest prescaler that gives the bit rate exactly and the sample point closest to
 * the one asked, with the synchronization jump width as long as both phase segments allow.
 *
 * @param limits Bit timing limits.
 * @param clock Clock the time quanta are counted in, in Hz.
 * @param bitrate Bit rate, in bits per second.
 * @param sample_point Sample point, in tenths of a percent.
 * @param[out] timing Bit timing.
 * @return 0, or @c -EINVAL if no timing within @p limits gives the bit rate.
 */
int pbl_can_calc_timing(const struct pbl_can_timing_limits *limits, uint32_t clock,
                        uint32_t bitrate, uint16_t sample_point, struct pbl_can_timing *timing);

/**
 * @brief Get the bytes of a frame of a data length code.
 *
 * @param dlc Data length code, 0 to 15.
 * @return Data bytes, as a CAN FD frame has them.
 */
uint8_t pbl_can_dlc_to_len(uint8_t dlc);

/**
 * @brief Get the data length code of a frame of @p len bytes.
 *
 * @param len Data bytes, one a CAN FD frame can have.
 * @return Data length code.
 */
uint8_t pbl_can_len_to_dlc(uint8_t len);

/**
 * @brief Set the bit rates and mode, while stopped.
 *
 * @param can Controller.
 * @param cfg Configuration.
 * @return 0, @c -EINVAL if the controller's clock cannot give a bit rate, or another negative
 * errno.
 */
int pbl_can_configure(const struct pbl_can *can, const struct pbl_can_config *cfg);

/**
 * @brief Get the configuration.
 *
 * @param can Controller.
 * @param[out] cfg Configuration.
 * @return 0, @c -EINVAL if the controller was never configured, or another negative errno.
 */
int pbl_can_config_get(const struct pbl_can *can, struct pbl_can_config *cfg);

/**
 * @brief Add an acceptance filter, while stopped.
 *
 * Once a filter is added, the controller only accepts the frames some filter matches.
 *
 * @param can Controller.
 * @param filter Filter.
 * @return Filter index, to remove it with, @c -ENOSPC if every filter is in use, or another
 * negative errno.
 */
int pbl_can_add_filter(const struct pbl_can *can, const struct pbl_can_filter *filter);

/**
 * @brief Remove an acceptance filter, while stopped.
 *
 * @param can Controller.
 * @param idx Filter index, from pbl_can_add_filter().
 * @return 0 or a negative errno.
 */
int pbl_can_remove_filter(const struct pbl_can *can, int idx);

/**
 * @brief Set the receive callback.
 *
 * @param can Controller.
 * @param cb Receive callback, or NULL to drop the frames received.
 * @param arg Callback argument.
 */
void pbl_can_set_rx_callback(const struct pbl_can *can, pbl_can_rx_callback_t cb, void *arg);

/**
 * @brief Set the state callback.
 *
 * @param can Controller.
 * @param cb State callback, or NULL.
 * @param arg Callback argument.
 */
void pbl_can_set_state_callback(const struct pbl_can *can, pbl_can_state_callback_t cb, void *arg);

/**
 * @brief Join the bus, with the configuration and filters given while stopped.
 *
 * @param can Controller.
 * @return 0, @c -EINVAL if the controller was never configured, or another negative errno.
 */
int pbl_can_start(const struct pbl_can *can);

/**
 * @brief Leave the bus. The frames in flight complete with @c -ENETDOWN.
 *
 * @param can Controller.
 * @return 0 or a negative errno.
 */
int pbl_can_stop(const struct pbl_can *can);

/**
 * @brief Send a frame.
 *
 * Waits up to @p timeout for room in the controller. With a callback, returns once the frame is
 * queued, and the callback gets its outcome. Without one, also waits up to @p timeout for the
 * frame to go, aborting it past then, and returns its outcome.
 *
 * @param can Controller.
 * @param frame Frame.
 * @param timeout How long to wait, @ref PBL_NO_WAIT from interrupt context.
 * @param cb Transmit callback, or NULL to wait for the frame to go. Required from interrupt
 * context.
 * @param arg Callback argument.
 * @return 0, @c -EPERM with @ref PBL_CAN_MODE_LISTEN_ONLY, @c -EINVAL for a frame the
 * configuration does not allow, @c -EAGAIN if the controller had no room in time,
 * @c -ETIMEDOUT if the frame did not go in time, or the outcome of the frame as given to
 * @ref pbl_can_tx_callback_t.
 */
int pbl_can_send(const struct pbl_can *can, const struct pbl_can_frame *frame,
                 pbl_timeout_t timeout, pbl_can_tx_callback_t cb, void *arg);

/**
 * @brief Get the state and error counters.
 *
 * @param can Controller.
 * @param[out] state State.
 * @param[out] cnt Error counters, or NULL.
 * @return 0 or a negative errno.
 */
int pbl_can_get_state(const struct pbl_can *can, enum pbl_can_bus_state *state,
                      struct pbl_can_err_cnt *cnt);

/**
 * @brief Start the recovery from bus off, with @ref PBL_CAN_MODE_MANUAL_RECOVERY.
 *
 * The controller rejoins the bus once it has seen 128 sequences of 11 recessive bits, and the
 * state callback reports it. Does nothing if the controller is not bus off.
 *
 * @param can Controller.
 * @return 0, @c -ENOTSUP without @ref PBL_CAN_MODE_MANUAL_RECOVERY, or another negative errno.
 */
int pbl_can_recover(const struct pbl_can *can);

/**
 * @brief Get the error counts.
 *
 * @param can Controller.
 * @param[out] stats Error counts.
 * @return 0 or a negative errno.
 */
int pbl_can_get_stats(const struct pbl_can *can, struct pbl_can_stats *stats);

/** @} */
