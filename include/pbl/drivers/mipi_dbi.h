/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>

/**
 * @defgroup drivers_mipi_dbi MIPI DBI
 * @ingroup drivers
 * @brief MIPI DBI hosts.
 *
 * A host is the controller a panel with its own frame memory is wired to, which sends the panel
 * its commands and the pixels of the display. The driver of the panel, a child of the host, sends
 * its own commands through this interface without knowing which host it is on: those of its power
 * sequence, and settings such as its brightness. The host sends the pixels.
 *
 * Commands are MIPI DCS commands, or the panel's own, with their parameters.
 * @{
 */

/** @name MIPI DCS commands */
/** @{ */
#define PBL_MIPI_DCS_SOFT_RESET             0x01U
#define PBL_MIPI_DCS_GET_DISPLAY_ID         0x04U
#define PBL_MIPI_DCS_ENTER_SLEEP_MODE       0x10U
#define PBL_MIPI_DCS_EXIT_SLEEP_MODE        0x11U
#define PBL_MIPI_DCS_SET_DISPLAY_OFF        0x28U
#define PBL_MIPI_DCS_SET_DISPLAY_ON         0x29U
#define PBL_MIPI_DCS_SET_COLUMN_ADDRESS     0x2AU
#define PBL_MIPI_DCS_SET_PAGE_ADDRESS       0x2BU
#define PBL_MIPI_DCS_WRITE_MEMORY_START     0x2CU
#define PBL_MIPI_DCS_SET_TEAR_ON            0x35U
#define PBL_MIPI_DCS_SET_ADDRESS_MODE       0x36U
#define PBL_MIPI_DCS_SET_PIXEL_FORMAT       0x3AU
#define PBL_MIPI_DCS_SET_DISPLAY_BRIGHTNESS 0x51U
#define PBL_MIPI_DCS_WRITE_CONTROL_DISPLAY  0x53U
/** @} */

struct pbl_mipi_dbi;

/** @brief Driver operations. */
struct pbl_mipi_dbi_ops {
  /** Send a command and its @p len parameters. Returns 0 or a negative errno. */
  int (*command_write)(const struct pbl_mipi_dbi *dbi, uint8_t cmd, const uint8_t *data,
                       size_t len);
  /** Send a command and read @p len bytes back. Returns 0 or a negative errno. */
  int (*command_read)(const struct pbl_mipi_dbi *dbi, uint8_t cmd, uint8_t *data, size_t len);
};

/** @brief MIPI DBI host. */
struct pbl_mipi_dbi {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_mipi_dbi_ops *ops;
};

/** @brief Class of the MIPI DBI hosts. */
extern const struct pbl_device_class pbl_mipi_dbi_class;

/**
 * @brief Initializer for the struct pbl_mipi_dbi of host @p sym.
 *
 * The host has no init of its own: it is a function of its controller, which sets it up before
 * it brings the panel up.
 *
 * @param sym Symbol of the host instance, with its state defined by PBL_DEVICE_STATE_DEFINE().
 * @param _name Name.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_MIPI_DBI_INIT(sym, _name, _parent, _ops)                                 \
  {                                                                                  \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, nullptr, _parent, &pbl_mipi_dbi_class), \
    .ops = (_ops),                                                                   \
  }

/**
 * @brief Send a command to the panel.
 *
 * @param dbi Host, whose controller is set up.
 * @param cmd Command.
 * @param data Parameters, or NULL.
 * @param len Number of parameters.
 * @return 0 or a negative errno.
 */
static inline int pbl_mipi_dbi_command_write(const struct pbl_mipi_dbi *dbi, uint8_t cmd,
                                             const uint8_t *data, size_t len) {
  return dbi->ops->command_write(dbi, cmd, data, len);
}

/**
 * @brief Send a command to the panel and read its answer.
 *
 * @param dbi Host, whose controller is set up.
 * @param cmd Command.
 * @param[out] data Bytes read.
 * @param len Number of bytes to read.
 * @return 0 or a negative errno.
 */
static inline int pbl_mipi_dbi_command_read(const struct pbl_mipi_dbi *dbi, uint8_t cmd,
                                            uint8_t *data, size_t len) {
  return dbi->ops->command_read(dbi, cmd, data, len);
}

/** @} */
