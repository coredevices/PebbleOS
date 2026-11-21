/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/drivers/mipi_dcs.h>
#include <pbl/drivers/panel.h>
#include <pbl/util/bits.h>

#include <sys/types.h>

/**
 * @defgroup drivers_mipi_dsi MIPI DSI
 * @ingroup drivers
 * @brief MIPI DSI hosts.
 *
 * A host is the controller a panel is wired to over MIPI DSI. The driver of the panel, a child
 * of the host, attaches to it with what the panel takes: its lanes, its pixel format, video or
 * command mode, and in video mode its timing. It then sends its own commands through the host,
 * those of its power sequence and its settings, as DCS or generic packets on its virtual
 * channel. In video mode, the host streams the pixels a display controller feeds it.
 *
 * @code{.c}
 * ret = pbl_mipi_dsi_attach(&s_dsi);
 * ret = pbl_mipi_dsi_dcs_write(&s_dsi, PBL_MIPI_DCS_EXIT_SLEEP_MODE, nullptr, 0);
 * @endcode
 * @{
 */

/** @name Data types of the packets a host sends */
/** @{ */
#define PBL_MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM    0x03U
#define PBL_MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM   0x04U
#define PBL_MIPI_DSI_DCS_SHORT_WRITE                0x05U
#define PBL_MIPI_DSI_DCS_READ                       0x06U
#define PBL_MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM    0x13U
#define PBL_MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM   0x14U
#define PBL_MIPI_DSI_DCS_SHORT_WRITE_PARAM          0x15U
#define PBL_MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM    0x23U
#define PBL_MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM   0x24U
#define PBL_MIPI_DSI_GENERIC_LONG_WRITE             0x29U
#define PBL_MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE 0x37U
#define PBL_MIPI_DSI_DCS_LONG_WRITE                 0x39U
/** @} */

/** @brief Most parameters of a DCS command pbl_mipi_dsi_dcs_write() sends. */
#define PBL_MIPI_DSI_DCS_MAX_PARAMS 63U

/** @brief Pixel formats of the video stream. */
enum pbl_mipi_dsi_pixel_format {
  /** 24 bits per pixel, 8 per color. */
  PBL_MIPI_DSI_FMT_RGB888,
  /** 18 bits per pixel, each in 3 bytes. */
  PBL_MIPI_DSI_FMT_RGB666,
  /** 18 bits per pixel, packed. */
  PBL_MIPI_DSI_FMT_RGB666_PACKED,
  /** 16 bits per pixel. */
  PBL_MIPI_DSI_FMT_RGB565,
};

/** @name Mode flags */
/** @{ */
/** Video mode: the host streams pixels, else the panel has its own frame memory. */
#define PBL_MIPI_DSI_MODE_VIDEO PBL_BIT(0)
/** In video mode, lines in bursts at the full rate of the link, low power in between. */
#define PBL_MIPI_DSI_MODE_VIDEO_BURST PBL_BIT(1)
/** In video mode, sync pulses with their ends, else sync events. */
#define PBL_MIPI_DSI_MODE_VIDEO_SYNC_PULSE PBL_BIT(2)
/** Commands in low power, else at high speed. */
#define PBL_MIPI_DSI_MODE_LPM PBL_BIT(3)
/** @} */

struct pbl_mipi_dsi_host;

/** @brief A panel on a host. */
struct pbl_mipi_dsi_device {
  /** Host. */
  const struct pbl_mipi_dsi_host *host;
  /** Virtual channel, 0 to 3. */
  uint8_t channel;
  /** Data lanes. */
  uint8_t lanes;
  /** Pixel format. */
  enum pbl_mipi_dsi_pixel_format format;
  /** Mode flags, @c PBL_MIPI_DSI_MODE_*. */
  uint32_t mode_flags;
  /** Timing in video mode, the clock being the pixel clock. */
  const struct pbl_panel_timing *timing;
};

/** @brief A packet to send, and what to read back. */
struct pbl_mipi_dsi_msg {
  /** Virtual channel. */
  uint8_t channel;
  /** Data type. */
  uint8_t type;
  /** Send in low power. */
  bool lpm;
  /** Payload, the parameters of a short packet or the data of a long one. */
  const uint8_t *tx_buf;
  /** Payload length. */
  size_t tx_len;
  /** Buffer for the response to a read, or NULL. */
  uint8_t *rx_buf;
  /** Bytes to read back. */
  size_t rx_len;
};

/** @brief Host operations. */
struct pbl_mipi_dsi_host_ops {
  /** Set the link up for a device and start it. Returns 0 or a negative errno. */
  int (*attach)(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_device *dsi);
  /** Stop the link. Returns 0 or a negative errno. */
  int (*detach)(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_device *dsi);
  /** Send a packet and read the response. Returns the bytes read or sent, or a negative errno. */
  ssize_t (*transfer)(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_msg *msg);
};

/** @brief Host. */
struct pbl_mipi_dsi_host {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_mipi_dsi_host_ops *ops;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_mipi_dsi_class;
/** @endcond */

/**
 * @brief Initializer of a host.
 *
 * @param sym Symbol of the host.
 * @param _name Name.
 * @param _init Device initialization function, or NULL.
 * @param _parent Parent device, or NULL.
 * @param _ops Driver operations.
 */
#define PBL_MIPI_DSI_HOST_INIT(sym, _name, _init, _parent, _ops)                   \
  {                                                                                \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, _init, _parent, &pbl_mipi_dsi_class), \
    .ops = (_ops),                                                                 \
  }

/**
 * @brief Set the link up for a device and start it.
 *
 * @param dsi Device.
 * @return 0, @c -ENOTSUP for a setting the host cannot take, or another negative errno.
 */
int pbl_mipi_dsi_attach(const struct pbl_mipi_dsi_device *dsi);

/**
 * @brief Stop the link of a device.
 *
 * @param dsi Device.
 * @return 0 or a negative errno.
 */
int pbl_mipi_dsi_detach(const struct pbl_mipi_dsi_device *dsi);

/**
 * @brief Send a DCS command with its parameters, as the shortest packet that holds them.
 *
 * @param dsi Device.
 * @param cmd Command.
 * @param data Parameters, or NULL.
 * @param len Number of parameters, up to @ref PBL_MIPI_DSI_DCS_MAX_PARAMS.
 * @return 0 or a negative errno.
 */
int pbl_mipi_dsi_dcs_write(const struct pbl_mipi_dsi_device *dsi, uint8_t cmd, const uint8_t *data,
                           size_t len);

/**
 * @brief Send a DCS command that reads, and read its response.
 *
 * @param dsi Device.
 * @param cmd Command.
 * @param data Buffer for the response.
 * @param len Bytes to read.
 * @return 0, @c -EIO for a shorter response, or another negative errno.
 */
int pbl_mipi_dsi_dcs_read(const struct pbl_mipi_dsi_device *dsi, uint8_t cmd, uint8_t *data,
                          size_t len);

/**
 * @brief Send generic data, as the shortest packet that holds it.
 *
 * @param dsi Device.
 * @param data Data.
 * @param len Its length.
 * @return 0 or a negative errno.
 */
int pbl_mipi_dsi_generic_write(const struct pbl_mipi_dsi_device *dsi, const uint8_t *data,
                               size_t len);

/** @} */
