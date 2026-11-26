/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/spi.h>
#include <pbl/drivers/spi_mem.h>
#include <pbl/drivers/spi_mem/spi.h>
#include <pbl/kernel/mutex.h>
#include <pbl/util/misc.h>

static const struct pbl_spi_mem_spi *prv_mem(const struct pbl_spi_mem *mem) {
  return PBL_CONTAINER_OF(mem, const struct pbl_spi_mem_spi, mem);
}

static int prv_init(const struct pbl_spi_mem *mem) {
  const struct pbl_spi_mem_spi *spi = prv_mem(mem);
  int ret;

  ret = pbl_device_init(&spi->dev.bus->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_spi_setup(&spi->dev);
}

static bool prv_supports_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  return op->cmd.buswidth == 1U && (op->addr.nbytes == 0U || op->addr.buswidth == 1U) &&
         op->addr.nbytes <= 4U && (op->dummy.nbytes == 0U || op->dummy.buswidth == 1U) &&
         op->dummy.nbytes <= 8U &&
         (op->data.dir == PBL_SPI_MEM_NO_DATA || op->data.buswidth == 1U) &&
         op->data.nbytes <= PBL_SPI_MEM_SPI_BUF_SIZE;
}

static int prv_adjust_op_size(const struct pbl_spi_mem *mem, struct pbl_spi_mem_op *op) {
  if (op->data.nbytes > PBL_SPI_MEM_SPI_BUF_SIZE) {
    op->data.nbytes = PBL_SPI_MEM_SPI_BUF_SIZE;
  }

  return 0;
}

static int prv_exec_op(const struct pbl_spi_mem *mem, const struct pbl_spi_mem_op *op) {
  const struct pbl_spi_mem_spi *spi = prv_mem(mem);
  struct pbl_spi_mem_spi_data *data = spi->data;
  struct pbl_spi_xfer xfers[2] = {0};
  size_t num = 1U;
  size_t len = 0U;
  int ret;

  pbl_mutex_lock(&data->lock, PBL_FOREVER);

  data->hdr[len++] = op->cmd.opcode;
  for (uint8_t i = op->addr.nbytes; i > 0U; i--) {
    data->hdr[len++] = (uint8_t)(op->addr.val >> (8U * (i - 1U)));
  }
  memset(&data->hdr[len], 0xff, op->dummy.nbytes);
  len += op->dummy.nbytes;

  xfers[0].tx_buf = data->hdr;
  xfers[0].len = len;

  // The data goes through a buffer, which the controller reaches whatever the caller passed
  if (op->data.dir == PBL_SPI_MEM_DATA_OUT && op->data.nbytes > 0U) {
    memcpy(data->buf, op->data.buf.out, op->data.nbytes);
    xfers[1].tx_buf = data->buf;
    xfers[1].len = op->data.nbytes;
    num++;
  } else if (op->data.dir == PBL_SPI_MEM_DATA_IN && op->data.nbytes > 0U) {
    xfers[1].rx_buf = data->buf;
    xfers[1].len = op->data.nbytes;
    num++;
  }

  ret = pbl_spi_transfer(&spi->dev, xfers, num);
  if (ret == 0 && op->data.dir == PBL_SPI_MEM_DATA_IN && op->data.nbytes > 0U) {
    memcpy(op->data.buf.in, data->buf, op->data.nbytes);
  }

  pbl_mutex_unlock(&data->lock);

  return ret;
}

const struct pbl_spi_mem_ops pbl_spi_mem_spi_ops = {
  .init = prv_init,
  .supports_op = prv_supports_op,
  .adjust_op_size = prv_adjust_op_size,
  .exec_op = prv_exec_op,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_spim_spi_mem)
