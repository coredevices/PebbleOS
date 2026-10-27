/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_dma DMA
 * @ingroup drivers
 * @brief DMA controller class.
 *
 * A reduced Linux dmaengine for peripheral (slave) transfers. A DMA controller is a struct
 * pbl_dma_controller device with numbered channels. A consumer holds a struct pbl_dma_chan, what
 * a devicetree @c dmas entry describes: controller, channel, request line of the peripheral and
 * controller-specific configuration such as the priority. It sets the peripheral side of the
 * channel up once, then starts one transfer at a time on a memory buffer:
 *
 * @code{.c}
 * static const struct pbl_dma_chan s_rx =
 *     PBL_DMA_CHAN(SF32LB52_DMAC1, 5, SF32LB52_DMA_REQ_I2C2, SF32LB52_DMA_PL_HIGH);
 *
 * const struct pbl_dma_slave_config cfg = {
 *   .src_addr = fifo_addr,
 *   .src_addr_width = PBL_DMA_SLAVE_BUSWIDTH_1_BYTE,
 * };
 *
 * ret = pbl_dma_slave_config(&s_rx, &cfg);
 *
 * ret = pbl_dma_prep_slave_single(&s_rx, buf, len, PBL_DMA_DEV_TO_MEM, nullptr, nullptr);
 * ...
 * ret = pbl_dma_terminate(&s_rx);
 * @endcode
 *
 * Unlike dmaengine there is no descriptor queue: preparing a transfer starts it, and a channel
 * runs one transfer at a time. The memory side uses the peripheral's bus width, so lengths are a
 * multiple of it. Buffers must be reachable by the controller; cache maintenance is up to the
 * consumer. Calls may be made from interrupt context; a channel belongs to one consumer.
 * @{
 */

struct pbl_dma_controller;

/** @brief Class of the DMA controllers. */
extern const struct pbl_device_class pbl_dma_class;

/** @brief Transfer direction. */
enum pbl_dma_dir {
  /** Memory to the peripheral. */
  PBL_DMA_MEM_TO_DEV,
  /** Peripheral to memory. */
  PBL_DMA_DEV_TO_MEM,
};

/** @brief Peripheral register width, in bytes. */
enum pbl_dma_slave_buswidth {
  /** 1 byte. */
  PBL_DMA_SLAVE_BUSWIDTH_1_BYTE = 1,
  /** 2 bytes. */
  PBL_DMA_SLAVE_BUSWIDTH_2_BYTES = 2,
  /** 4 bytes. */
  PBL_DMA_SLAVE_BUSWIDTH_4_BYTES = 4,
};

/** @brief Peripheral side of a channel. */
struct pbl_dma_slave_config {
  /** Peripheral register read from, for @ref PBL_DMA_DEV_TO_MEM transfers. */
  uintptr_t src_addr;
  /** Peripheral register written to, for @ref PBL_DMA_MEM_TO_DEV transfers. */
  uintptr_t dst_addr;
  /** Width of @ref src_addr. */
  enum pbl_dma_slave_buswidth src_addr_width;
  /** Width of @ref dst_addr. */
  enum pbl_dma_slave_buswidth dst_addr_width;
};

/**
 * @brief Transfer end callback, called from interrupt context.
 *
 * @param arg Callback argument.
 * @param status 0 when the transfer completed, or a negative errno.
 */
typedef void (*pbl_dma_callback_t)(void *arg, int status);

/** @brief A channel, as used by a consumer. */
struct pbl_dma_chan {
  /** Controller. */
  const struct pbl_dma_controller *ctrl;
  /** Channel number, from 0. */
  uint8_t channel;
  /** Request line of the peripheral, from the controller's header. */
  uint8_t request;
  /** Controller-specific configuration, from the controller's header. */
  uint32_t config;
};

/**
 * @brief Initializer for a struct pbl_dma_chan.
 *
 * @param _ctrl Controller.
 * @param _channel Channel number, from 0.
 * @param _request Request line of the peripheral.
 * @param _config Controller-specific configuration.
 */
#define PBL_DMA_CHAN(_ctrl, _channel, _request, _config) \
  {.ctrl = (_ctrl), .channel = (_channel), .request = (_request), .config = (_config)}

/** @brief DMA controller driver operations. */
struct pbl_dma_controller_ops {
  /** Set the peripheral side of a stopped channel up. Returns 0 or a negative errno. */
  int (*slave_config)(const struct pbl_dma_controller *ctrl, const struct pbl_dma_chan *chan,
                      const struct pbl_dma_slave_config *cfg);
  /** Start a transfer on a configured channel. Returns 0 or a negative errno. */
  int (*prep_slave_single)(const struct pbl_dma_controller *ctrl, uint8_t channel, void *buf,
                           size_t len, enum pbl_dma_dir dir);
  /** Stop a channel. Returns 0 or a negative errno. */
  int (*terminate)(const struct pbl_dma_controller *ctrl, uint8_t channel);
  /** Read the bytes left of a channel's transfer. Returns 0 or a negative errno. */
  int (*tx_status)(const struct pbl_dma_controller *ctrl, uint8_t channel, size_t *residue);
};

/** @brief Runtime state of a channel. */
struct pbl_dma_chan_state {
  /** Peripheral side, from pbl_dma_slave_config(). */
  struct pbl_dma_slave_config cfg;
  /** Transfer end callback, or NULL. */
  pbl_dma_callback_t callback;
  /** Callback argument. */
  void *arg;
};

/** @brief A DMA controller. */
struct pbl_dma_controller {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_dma_controller_ops *ops;
  /** State of each channel. */
  struct pbl_dma_chan_state *chans;
  /** Number of channels. */
  uint8_t num_channels;
};

/**
 * @brief Define the class state of DMA controller @p sym, for the driver's define macro.
 *
 * @param sym Symbol of the controller instance.
 * @param _num_channels Number of channels.
 */
#define PBL_DMA_CONTROLLER_STATE_DEFINE(sym, _num_channels) \
  PBL_DEVICE_STATE_DEFINE(sym);                             \
  static struct pbl_dma_chan_state sym##_dma_chans[_num_channels]

/**
 * @brief Initializer for the struct pbl_dma_controller of controller @p sym.
 *
 * @param sym Symbol of the controller instance, with its state defined by
 * PBL_DMA_CONTROLLER_STATE_DEFINE().
 * @param _name Name.
 * @param _init Device init function, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_DMA_CONTROLLER_INIT(sym, _name, _init, _ops)                               \
  {                                                                                    \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, nullptr, nullptr, &pbl_dma_class), \
    .ops = (_ops),                                                                     \
    .chans = sym##_dma_chans,                                                          \
    .num_channels = sizeof(sym##_dma_chans) / sizeof(sym##_dma_chans[0]),              \
  }

/**
 * @brief Set the peripheral side of a stopped channel up.
 *
 * @param chan Channel.
 * @param cfg Peripheral side.
 * @return 0, @c -EINVAL for a configuration the controller cannot do, or another negative errno.
 */
int pbl_dma_slave_config(const struct pbl_dma_chan *chan, const struct pbl_dma_slave_config *cfg);

/**
 * @brief Start a transfer between a buffer and the peripheral.
 *
 * @param chan Channel, set up with pbl_dma_slave_config().
 * @param buf Buffer.
 * @param len Length in bytes, a multiple of the peripheral's bus width.
 * @param dir Direction.
 * @param callback Called when the transfer ends, or NULL.
 * @param arg Callback argument.
 * @return 0, @c -EBUSY if a transfer is running, @c -EINVAL for a transfer the controller cannot
 * do, or another negative errno.
 */
int pbl_dma_prep_slave_single(const struct pbl_dma_chan *chan, void *buf, size_t len,
                              enum pbl_dma_dir dir, pbl_dma_callback_t callback, void *arg);

/**
 * @brief Stop a channel. No callback is called for the transfer it stops.
 *
 * @param chan Channel.
 * @return 0 or a negative errno.
 */
int pbl_dma_terminate(const struct pbl_dma_chan *chan);

/**
 * @brief Read the bytes left of a channel's transfer.
 *
 * @param chan Channel.
 * @param[out] residue Bytes not transferred yet.
 * @return 0 or a negative errno.
 */
int pbl_dma_tx_status(const struct pbl_dma_chan *chan, size_t *residue);

/**
 * @brief Report the end of a channel's transfer, calling its callback.
 *
 * Called by controller drivers from their interrupt handlers.
 *
 * @param ctrl Controller.
 * @param channel Channel.
 * @param status 0 when the transfer completed, or a negative errno.
 */
void pbl_dma_complete(const struct pbl_dma_controller *ctrl, uint8_t channel, int status);

/** @} */
