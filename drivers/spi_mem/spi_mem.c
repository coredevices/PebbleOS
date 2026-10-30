/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/spi_mem.h>
#include <pbl/kernel/delay.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

PBL_LOG_MODULE_DEFINE(driver_spi_mem, CONFIG_DRIVER_SPI_MEM_LOG_LEVEL);

PBL_DEVICE_CLASS_DEFINE(pbl_spi_mem_class, "spi_mem");

int pbl_spi_mem_dev_init(const struct pbl_device *dev) {
  const struct pbl_spi_mem *mem = container_of(dev, const struct pbl_spi_mem, dev);

  if (pbl_spi_mem_is_xip(mem) && mem->ops->poll_status == nullptr) {
    return -EINVAL;
  }

  if (mem->ops->init == nullptr) {
    return 0;
  }

  return mem->ops->init(mem);
}

bool pbl_spi_mem_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  return mem->ops->supports_op(mem, op);
}

int pbl_spi_mem_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op) {
  if (mem->ops->adjust_op_size == nullptr) {
    return 0;
  }

  return mem->ops->adjust_op_size(mem, op);
}

int pbl_spi_mem_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  if (!mem->ops->supports_op(mem, op)) {
    return -ENOTSUP;
  }

  return mem->ops->exec_op(mem, op);
}

int pbl_spi_mem_poll_status(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op,
                            const struct pbl_spi_mem_op *status, uint8_t mask, uint8_t match,
                            const struct pbl_spi_mem_suspend *susp) {
  if (mem->ops->poll_status == nullptr || !mem->ops->supports_op(mem, op) ||
      !mem->ops->supports_op(mem, status)) {
    return -ENOTSUP;
  }

  return mem->ops->poll_status(mem, op, status, mask, match, susp);
}

int pbl_spi_mem_dirmap_create(const struct pbl_spi_mem *mem, struct pbl_spi_mem_dirmap *desc) {
  struct pbl_spi_mem_op op = desc->op;
  int ret;

  op.addr.val = desc->offset;
  if (op.data.dir != PBL_SPI_MEM_DATA_IN || !mem->ops->supports_op(mem, &op)) {
    return -ENOTSUP;
  }

  desc->mapped = false;
  if (mem->ops->dirmap_create == nullptr) {
    return 0;
  }

  ret = mem->ops->dirmap_create(mem, desc);
  if (ret == -ENOTSUP) {
    return 0;
  }

  if (ret != 0) {
    return ret;
  }

  desc->mapped = true;

  return 0;
}

// Without a window, a read is the window's operation run in as many pieces as the controller needs
static int prv_read_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                       uint32_t offs, void *buf, size_t len) {
  struct pbl_spi_mem_op op = desc->op;
  uint8_t *dst = buf;
  int ret;

  while (len > 0U) {
    op.addr.val = offs;
    op.data.buf.in = dst;
    op.data.nbytes = len;

    ret = pbl_spi_mem_adjust_op_size(mem, &op);
    if (ret != 0) {
      return ret;
    }

    if (op.data.nbytes == 0U) {
      return -EINVAL;
    }

    ret = mem->ops->exec_op(mem, &op);
    if (ret != 0) {
      return ret;
    }

    offs += op.data.nbytes;
    dst += op.data.nbytes;
    len -= op.data.nbytes;
  }

  return 0;
}

int pbl_spi_mem_dirmap_read(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_dirmap *desc,
                            uint32_t offs, void *buf, size_t len) {
  if (offs < desc->offset || len > desc->length || (offs - desc->offset) > (desc->length - len)) {
    return -EINVAL;
  }

  if (desc->mapped) {
    return mem->ops->dirmap_read(mem, desc, offs, buf, len);
  }

  return prv_read_op(mem, desc, offs, buf, len);
}

void pbl_spi_mem_dirmap_invalidate(const struct pbl_spi_mem *mem,
                                   const struct pbl_spi_mem_dirmap *desc, uint32_t offs,
                                   size_t len) {
  if (desc->mapped && mem->ops->dirmap_invalidate != nullptr) {
    mem->ops->dirmap_invalidate(mem, desc, offs, len);
  }
}

void pbl_spi_mem_power_down(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_pm *pm,
                            pbl_spi_mem_pm_fn_t fn, void *arg) {
  if (mem->ops->power_down != nullptr) {
    mem->ops->power_down(mem, pm, fn, arg);
    return;
  }

  if (pm != nullptr) {
    (void)pbl_spi_mem_exec_op(mem, &pm->enter);
    pbl_busy_wait_us(pm->enter_us);
  }

  fn(arg);

  if (pm != nullptr) {
    (void)pbl_spi_mem_exec_op(mem, &pm->exit);
    pbl_busy_wait_us(pm->exit_us);
  }
}

void pbl_spi_mem_set_polling(const struct pbl_spi_mem *mem, bool polling) {
  mem->state->polling = polling;
}
