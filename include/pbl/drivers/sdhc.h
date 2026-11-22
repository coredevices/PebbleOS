/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/bits.h>

/**
 * @defgroup drivers_sdhc SD host controller
 * @ingroup drivers
 * @brief SD host controller class: the bus of an SD card.
 *
 * A host controller runs SD commands on its bus, each with an optional data phase of whole
 * blocks, at the clock and bus width the card stack sets. The card stack, not the controller,
 * knows the protocol: it brings the card up and moves its blocks through pbl_sdhc_request().
 *
 * @code{.c}
 * struct pbl_sdhc_cmd cmd = {.opcode = 0, .flags = PBL_SDHC_RSP_NONE};
 *
 * ret = pbl_sdhc_set_ios(SDHC, &(struct pbl_sdhc_ios){.clock_hz = 400000U, .bus_width = 1U});
 * ret = pbl_sdhc_request(SDHC, &cmd, nullptr);
 * @endcode
 * @{
 */

struct pbl_sdhc;

/** @brief Class of the SD host controllers. */
extern const struct pbl_device_class pbl_sdhc_class;

/** @brief The board's SD host controller, its @c sdhc role. */
extern const struct pbl_sdhc *const SDHC;

/** @brief The command has a response. */
#define PBL_SDHC_RSP_PRESENT PBL_BIT(0)
/** @brief The response is 136 bits long. */
#define PBL_SDHC_RSP_136 PBL_BIT(1)
/** @brief The response carries a valid CRC. */
#define PBL_SDHC_RSP_CRC PBL_BIT(2)
/** @brief The card signals busy on DAT0 after the response. */
#define PBL_SDHC_RSP_BUSY PBL_BIT(3)

/** @brief No response. */
#define PBL_SDHC_RSP_NONE 0U
/** @brief R1, R5, R6 and R7: 48 bits with CRC. */
#define PBL_SDHC_RSP_R1 (PBL_SDHC_RSP_PRESENT | PBL_SDHC_RSP_CRC)
/** @brief R1b: R1, then busy. */
#define PBL_SDHC_RSP_R1B (PBL_SDHC_RSP_R1 | PBL_SDHC_RSP_BUSY)
/** @brief R2: 136 bits, the CID or CSD. */
#define PBL_SDHC_RSP_R2 (PBL_SDHC_RSP_PRESENT | PBL_SDHC_RSP_136 | PBL_SDHC_RSP_CRC)
/** @brief R3: 48 bits without CRC, the OCR. */
#define PBL_SDHC_RSP_R3 PBL_SDHC_RSP_PRESENT

/** @brief A command. */
struct pbl_sdhc_cmd {
  /** Command index. */
  uint8_t opcode;
  /** Argument. */
  uint32_t arg;
  /** Response type, PBL_SDHC_RSP_*. */
  uint32_t flags;
  /** Response, filled in by the controller: bits 127:96 of a long one first, else one word. */
  uint32_t resp[4];
};

/** @brief The data phase of a command, in whole blocks. */
struct pbl_sdhc_data {
  /** Buffer, read into or written from. */
  void *buf;
  /** Block size in bytes, a power of two. */
  uint32_t block_size;
  /** Number of blocks. */
  uint32_t blocks;
  /** Whether the data goes to the card. */
  bool write;
};

/** @brief Bus settings. */
struct pbl_sdhc_ios {
  /** Bus clock in Hz, at most this; 0 to stop it. */
  uint32_t clock_hz;
  /** Bus width, 1 or 4 lines. */
  uint8_t bus_width;
};

/** @brief Host controller operations, implemented by the drivers. */
struct pbl_sdhc_ops {
  /** Bring the controller up, with the card powered and the bus idle. */
  int (*init)(const struct pbl_sdhc *sdhc);
  /** Set the bus clock and width. */
  int (*set_ios)(const struct pbl_sdhc *sdhc, const struct pbl_sdhc_ios *ios);
  /** Run a command, with its data phase if @p data is not @c nullptr. */
  int (*request)(const struct pbl_sdhc *sdhc, struct pbl_sdhc_cmd *cmd,
                 const struct pbl_sdhc_data *data);
  /** Optional. Whether a card is in the slot, 1 or 0, or a negative errno. */
  int (*get_cd)(const struct pbl_sdhc *sdhc);
};

/** @brief Host controller state, kept by the class. */
struct pbl_sdhc_state {
  /** Serializes the commands. */
  struct pbl_mutex lock;
};

/** @brief SD host controller. */
struct pbl_sdhc {
  /** Device. */
  struct pbl_device dev;
  /** Operations. */
  const struct pbl_sdhc_ops *ops;
  /** State. */
  struct pbl_sdhc_state *state;
  /** Widest bus the slot is wired for, 1 or 4 lines. */
  uint8_t bus_width;
  /** Fastest bus clock of the slot, in Hz. */
  uint32_t max_clock_hz;
  /** Most blocks the data phase of one command moves, 0 for no limit. */
  uint16_t max_blocks;
  /** Alignment of the buffers of data phases of more than one block, 0 for any. */
  uint16_t multi_block_align;
};

/** @cond INTERNAL_HIDDEN */
int pbl_sdhc_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Define the state of a host controller, for drivers.
 *
 * @param sym Host controller symbol.
 */
#define PBL_SDHC_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);    \
  static struct pbl_sdhc_state sym##_sdhc_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initializer of a struct pbl_sdhc, for drivers.
 *
 * @param sym Host controller symbol, with its PBL_SDHC_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device, or NULL.
 * @param _ops Operations.
 * @param _bus_width Widest bus the slot is wired for.
 * @param _max_clock_hz Fastest bus clock of the slot.
 * @param _max_blocks Most blocks the data phase of one command moves, 0 for no limit.
 * @param _multi_block_align Alignment of the buffers of data phases of more than one block, 0 for
 *        any.
 */
#define PBL_SDHC_INIT(sym, _name, _parent, _ops, _bus_width, _max_clock_hz, _max_blocks,   \
                      _multi_block_align)                                                  \
  {                                                                                        \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_sdhc_dev_init, _parent, &pbl_sdhc_class), \
    .ops = (_ops),                                                                         \
    .state = &sym##_sdhc_state,                                                            \
    .bus_width = (_bus_width),                                                             \
    .max_clock_hz = (_max_clock_hz),                                                       \
    .max_blocks = (_max_blocks),                                                           \
    .multi_block_align = (_multi_block_align),                                             \
  }

/**
 * @brief Set the bus clock and width.
 *
 * @param sdhc Host controller.
 * @param ios Settings.
 * @return 0, -ENODEV if the controller is not ready, or a negative errno.
 */
int pbl_sdhc_set_ios(const struct pbl_sdhc *sdhc, const struct pbl_sdhc_ios *ios);

/**
 * @brief Run a command, with its data phase.
 *
 * Waits, so call it from thread context.
 *
 * @param sdhc Host controller.
 * @param cmd Command, whose response the controller fills in.
 * @param data Data phase, or @c nullptr.
 * @return 0, -ENODEV if the controller is not ready, -ETIMEDOUT if the card did not answer,
 *         -EILSEQ on a CRC error, or a negative errno.
 */
int pbl_sdhc_request(const struct pbl_sdhc *sdhc, struct pbl_sdhc_cmd *cmd,
                     const struct pbl_sdhc_data *data);

/**
 * @brief Whether a card is in the slot.
 *
 * @param sdhc Host controller.
 * @return 1 or 0, 1 when the slot cannot tell, or a negative errno.
 */
int pbl_sdhc_get_cd(const struct pbl_sdhc *sdhc);

/** @} */
