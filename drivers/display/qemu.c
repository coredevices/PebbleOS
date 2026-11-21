/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display/qemu.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#define DISP_CTRL       0x000
#define DISP_WIDTH      0x008
#define DISP_HEIGHT     0x00C
#define DISP_FORMAT     0x010
#define DISP_FLAGS      0x014
#define DISP_INT_STATUS 0x01C
#define DISP_MASK       0x400
#define DISP_MASK_SIZE  512U

#define CTRL_ENABLE         (1 << 0)
#define CTRL_UPDATE_REQUEST (1 << 1)

#define INT_UPDATE_DONE_PENDING (1 << 0)

#define FLAGS_ROUND (1 << 0)

static const struct pbl_display_qemu *prv_qemu(const struct pbl_display *disp) {
  return PBL_CONTAINER_OF(disp, const struct pbl_display_qemu, disp);
}

static uint32_t prv_row_bytes(const struct pbl_display *disp) {
  if (disp->props.format == PBL_DISPLAY_FORMAT_MONO) {
    return PBL_ROUNDUP(disp->props.width, 32U) / 8U;
  }

  return disp->props.width;
}

static void prv_refresh(const struct pbl_display_qemu *qemu) {
  pbl_writel(pbl_readl(qemu->base + DISP_CTRL) | CTRL_UPDATE_REQUEST, qemu->base + DISP_CTRL);
}

// The peripheral takes the geometry while disabled, and applies it on enable
static int prv_set_geometry(const struct pbl_display_qemu *qemu) {
  const struct pbl_display_props *props = &qemu->disp.props;
  const uint32_t bpp = (props->format == PBL_DISPLAY_FORMAT_MONO) ? 1U : 8U;
  const uint32_t flags = (props->round_mask != nullptr) ? FLAGS_ROUND : 0U;
  const uint32_t rows = props->height / 2U;

  if (rows > DISP_MASK_SIZE) {
    return -EINVAL;
  }

  pbl_writel(props->width, qemu->base + DISP_WIDTH);
  pbl_writel(props->height, qemu->base + DISP_HEIGHT);
  pbl_writel(bpp, qemu->base + DISP_FORMAT);
  pbl_writel(flags, qemu->base + DISP_FLAGS);
  for (uint32_t row = 0U; row < DISP_MASK_SIZE; row += 4U) {
    uint32_t word = 0U;

    for (uint32_t i = 0U; i < 4U; i++) {
      if (props->round_mask != nullptr && row + i < rows) {
        word |= (uint32_t)props->round_mask[row + i] << (8U * i);
      }
    }
    pbl_writel(word, qemu->base + DISP_MASK + row);
  }

  pbl_writel(CTRL_ENABLE, qemu->base + DISP_CTRL);

  // A peripheral that keeps its own geometry would show the framebuffer wrong
  if (pbl_readl(qemu->base + DISP_WIDTH) != props->width ||
      pbl_readl(qemu->base + DISP_HEIGHT) != props->height ||
      pbl_readl(qemu->base + DISP_FORMAT) != bpp || pbl_readl(qemu->base + DISP_FLAGS) != flags) {
    return -ENODEV;
  }

  return 0;
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);

  pbl_writel(INT_UPDATE_DONE_PENDING, qemu->base + DISP_INT_STATUS);
  pbl_writel(0U, qemu->base + DISP_CTRL);

  return prv_set_geometry(qemu);
}

static int prv_enable(const struct pbl_display *disp) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);

  pbl_writel(pbl_readl(qemu->base + DISP_CTRL) | CTRL_ENABLE, qemu->base + DISP_CTRL);

  return 0;
}

static int prv_disable(const struct pbl_display *disp) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);

  pbl_writel(pbl_readl(qemu->base + DISP_CTRL) & ~CTRL_ENABLE, qemu->base + DISP_CTRL);

  return 0;
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  return 0;
}

static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);
  uint32_t row_bytes = prv_row_bytes(disp);

  for (uint16_t y = y0; y <= y1; y++) {
    memcpy((void *)(qemu->fb + (y * row_bytes)), &buf->data[y * buf->pitch], row_bytes);
  }

  prv_refresh(qemu);
  pbl_display_complete(disp, 0);

  return 0;
}

static int prv_clear(const struct pbl_display *disp) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);

  memset((void *)qemu->fb, 0, prv_row_bytes(disp) * disp->props.height);
  prv_refresh(qemu);

  return 0;
}

const struct pbl_display_ops pbl_display_qemu_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
  .clear = prv_clear,
};

#include PBL_DT_INSTANCES(qemu_pebble_display_display)
