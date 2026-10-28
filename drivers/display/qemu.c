/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/display/qemu.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#define DISP_CTRL       0x000
#define DISP_INT_STATUS 0x01C

#define CTRL_ENABLE         (1 << 0)
#define CTRL_UPDATE_REQUEST (1 << 1)

#define INT_UPDATE_DONE_PENDING (1 << 0)

static const struct pbl_display_qemu *prv_qemu(const struct pbl_display *disp) {
  return container_of(disp, const struct pbl_display_qemu, disp);
}

static uint32_t prv_row_bytes(const struct pbl_display *disp) {
  if (disp->format == PBL_DISPLAY_FORMAT_MONO) {
    return ROUND_TO_MOD_CEIL(disp->width, 32U) / 8U;
  }

  return disp->width;
}

static void prv_refresh(const struct pbl_display_qemu *qemu) {
  pbl_writel(pbl_readl(qemu->base + DISP_CTRL) | CTRL_UPDATE_REQUEST, qemu->base + DISP_CTRL);
}

static int prv_init(const struct pbl_display *disp) {
  const struct pbl_display_qemu *qemu = prv_qemu(disp);

  pbl_writel(INT_UPDATE_DONE_PENDING, qemu->base + DISP_INT_STATUS);
  pbl_writel(CTRL_ENABLE, qemu->base + DISP_CTRL);

  return 0;
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

  memset((void *)qemu->fb, 0, prv_row_bytes(disp) * disp->height);
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
