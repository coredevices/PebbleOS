/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/mipi_dsi.h>
#include <pbl/util/size.h>

PBL_DEVICE_CLASS_DEFINE(pbl_mipi_dsi_class, "mipi_dsi");

// A short packet holds up to 2 bytes
#define SHORT_MAX 2U

static ssize_t prv_transfer(const struct pbl_mipi_dsi_device *dsi,
                            const struct pbl_mipi_dsi_msg *msg) {
  if (!pbl_device_is_ready(&dsi->host->dev)) {
    return -ENODEV;
  }

  return dsi->host->ops->transfer(dsi->host, msg);
}

int pbl_mipi_dsi_attach(const struct pbl_mipi_dsi_device *dsi) {
  if (!pbl_device_is_ready(&dsi->host->dev)) {
    return -ENODEV;
  }

  if (dsi->channel > 3U || dsi->lanes == 0U ||
      ((dsi->mode_flags & PBL_MIPI_DSI_MODE_VIDEO) != 0U && dsi->timing == nullptr)) {
    return -EINVAL;
  }

  return dsi->host->ops->attach(dsi->host, dsi);
}

int pbl_mipi_dsi_detach(const struct pbl_mipi_dsi_device *dsi) {
  if (!pbl_device_is_ready(&dsi->host->dev)) {
    return -ENODEV;
  }

  return dsi->host->ops->detach(dsi->host, dsi);
}

static int prv_write(const struct pbl_mipi_dsi_device *dsi, uint8_t type, const uint8_t *data,
                     size_t len) {
  const struct pbl_mipi_dsi_msg msg = {
    .channel = dsi->channel,
    .type = type,
    .lpm = (dsi->mode_flags & PBL_MIPI_DSI_MODE_LPM) != 0U,
    .tx_buf = data,
    .tx_len = len,
  };
  ssize_t ret = prv_transfer(dsi, &msg);

  return ret < 0 ? (int)ret : 0;
}

int pbl_mipi_dsi_dcs_write(const struct pbl_mipi_dsi_device *dsi, uint8_t cmd, const uint8_t *data,
                           size_t len) {
  uint8_t buf[1U + SHORT_MAX];
  static const uint8_t s_types[] = {
    PBL_MIPI_DSI_DCS_SHORT_WRITE, PBL_MIPI_DSI_DCS_SHORT_WRITE_PARAM
  };

  if (len < PBL_ARRAY_SIZE(s_types)) {
    buf[0] = cmd;
    if (len == 1U) {
      buf[1] = data[0];
    }
    return prv_write(dsi, s_types[len], buf, 1U + len);
  }

  if (len > PBL_MIPI_DSI_DCS_MAX_PARAMS) {
    return -EINVAL;
  }

  // A long packet: the command, then its parameters, in one payload
  uint8_t payload[1U + PBL_MIPI_DSI_DCS_MAX_PARAMS];

  payload[0] = cmd;
  memcpy(&payload[1], data, len);

  return prv_write(dsi, PBL_MIPI_DSI_DCS_LONG_WRITE, payload, 1U + len);
}

int pbl_mipi_dsi_generic_write(const struct pbl_mipi_dsi_device *dsi, const uint8_t *data,
                               size_t len) {
  static const uint8_t s_types[] = {
    PBL_MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM, PBL_MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM,
    PBL_MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM
  };

  return prv_write(dsi,
                   len < PBL_ARRAY_SIZE(s_types) ? s_types[len] : PBL_MIPI_DSI_GENERIC_LONG_WRITE,
                   data, len);
}

int pbl_mipi_dsi_dcs_read(const struct pbl_mipi_dsi_device *dsi, uint8_t cmd, uint8_t *data,
                          size_t len) {
  const uint8_t size[SHORT_MAX] = {(uint8_t)len, (uint8_t)(len >> 8)};
  const struct pbl_mipi_dsi_msg msg = {
    .channel = dsi->channel,
    .type = PBL_MIPI_DSI_DCS_READ,
    .lpm = (dsi->mode_flags & PBL_MIPI_DSI_MODE_LPM) != 0U,
    .tx_buf = &cmd,
    .tx_len = 1U,
    .rx_buf = data,
    .rx_len = len,
  };
  ssize_t ret;

  // The panel returns no more than the host asks for
  ret = prv_write(dsi, PBL_MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, size, sizeof(size));
  if (ret != 0) {
    return (int)ret;
  }

  ret = prv_transfer(dsi, &msg);
  if (ret < 0) {
    return (int)ret;
  }

  return (size_t)ret == len ? 0 : -EIO;
}

int pbl_mipi_dsi_refresh(const struct pbl_mipi_dsi_host *host) {
  if (!pbl_device_is_ready(&host->dev)) {
    return -ENODEV;
  }

  if (host->ops->refresh == nullptr) {
    return -ENOTSUP;
  }

  return host->ops->refresh(host);
}

int pbl_mipi_dsi_set_event_cb(const struct pbl_mipi_dsi_host *host, pbl_mipi_dsi_event_cb_t cb,
                              void *arg) {
  if (!pbl_device_is_ready(&host->dev)) {
    return -ENODEV;
  }

  if (host->ops->set_event_cb == nullptr) {
    return -ENOTSUP;
  }

  host->ops->set_event_cb(host, cb, arg);

  return 0;
}

int pbl_mipi_dsi_hold(const struct pbl_mipi_dsi_host *host, bool hold) {
  if (host->ops->hold == nullptr) {
    return -ENOTSUP;
  }

  host->ops->hold(host, hold);

  return 0;
}
