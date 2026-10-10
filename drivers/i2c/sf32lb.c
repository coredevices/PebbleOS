/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/i2c/sf32lb.h>
#include <pbl/kernel/irq.h>
#include <pbl/mcu/cache.h>
#include <pbl/soc/sf32lb/sleep.h>
#include <pbl/util/misc.h>

#include <system/passert.h>

static const struct pbl_i2c_sf32lb *prv_i2c(const struct pbl_i2c_bus *bus) {
  return container_of(bus, const struct pbl_i2c_sf32lb, bus);
}

// Block deep sleep while a transfer is in flight. The flag keeps the release
// exactly-once across the IRQ, kickoff-failure and abort paths.
static void prv_deepsleep_block(const struct pbl_i2c_sf32lb *i2c) {
  i2c->state->deepsleep_blocked = true;
  soc_sf32lb_sleep_block(SOC_SF32LB_DEEPSLEEP);
}

static void prv_deepsleep_allow(const struct pbl_i2c_sf32lb *i2c) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  pbl_irq_lock();
  bool blocked = state->deepsleep_blocked;
  state->deepsleep_blocked = false;
  pbl_irq_unlock();

  if (blocked) {
    soc_sf32lb_sleep_release(SOC_SF32LB_DEEPSLEEP);
  }
}

static bool prv_dma_usable(const struct pbl_i2c_sf32lb *i2c,
                           const struct pbl_i2c_transfer *transfer) {
  return transfer->dma && i2c->state->hdma_rx.Instance != NULL && transfer->with_reg &&
         transfer->dir == PBL_I2C_READ && transfer->size >= I2C_SF32LB_DMA_MIN_BYTES;
}

//! Makes a completed DMA read visible to the CPU; stops an aborted one.
static void prv_dma_finish(const struct pbl_i2c_sf32lb *i2c, bool completed) {
  struct pbl_i2c_sf32lb_state *state = i2c->state;

  if (state->dma_data == NULL) {
    return;
  }

  if (completed) {
    dcache_invalidate(state->dma_data, DCACHE_ROUND_UP(state->dma_size));
  } else {
    HAL_DMA_Abort(&state->hdma_rx);
  }
  state->dma_data = NULL;
}

void pbl_i2c_sf32lb_irq_handler(const struct pbl_i2c_sf32lb *i2c) {
  I2C_HandleTypeDef *hdl = &i2c->state->hdl;
  HAL_I2C_StateTypeDef state;
  enum pbl_i2c_event event;

  if (hdl->XferISR == NULL) {
    return;
  }

  (void)hdl->XferISR(hdl, 0, 0);

  state = HAL_I2C_GetState(hdl);
  if ((state == HAL_I2C_STATE_BUSY_TX) || (state == HAL_I2C_STATE_BUSY_RX)) {
    return;
  } else if (state == HAL_I2C_STATE_READY) {
    event = PBL_I2C_EVENT_COMPLETE;
  } else {
    event = PBL_I2C_EVENT_ERROR;
  }

  prv_deepsleep_allow(i2c);
  prv_dma_finish(i2c, event == PBL_I2C_EVENT_COMPLETE);

  pbl_i2c_bus_event(&i2c->bus, event);
}

void pbl_i2c_sf32lb_dma_irq_handler(const struct pbl_i2c_sf32lb *i2c) {
  HAL_DMA_IRQHandler(&i2c->state->hdma_rx);
}

static void prv_begin_transfer(const struct pbl_i2c_bus *bus) {
  prv_deepsleep_block(prv_i2c(bus));
}

static void prv_abort_transfer(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);

  HAL_I2C_Reset(&i2c->state->hdl);

  prv_deepsleep_allow(i2c);
  prv_dma_finish(i2c, false);
}

static void prv_start_transfer(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  I2C_HandleTypeDef *hdl = &i2c->state->hdl;
  const struct pbl_i2c_transfer *transfer = &bus->state->transfer;
  HAL_StatusTypeDef ret;

  if (prv_dma_usable(i2c, transfer)) {
    // Dropping the buffer's lines now keeps a dirty one from being evicted over the DMA data.
    i2c->state->dma_data = transfer->data;
    i2c->state->dma_size = transfer->size;
    dcache_invalidate(transfer->data, DCACHE_ROUND_UP(transfer->size));
    HAL_DMA_Init(&i2c->state->hdma_rx);
    ret = HAL_I2C_Mem_Read_DMA(hdl, transfer->addr, transfer->reg, I2C_MEMADD_SIZE_8BIT,
                               transfer->data, transfer->size);
  } else if (transfer->with_reg) {
    if (transfer->dir == PBL_I2C_READ) {
      ret = HAL_I2C_Mem_Read_IT(hdl, transfer->addr, transfer->reg, I2C_MEMADD_SIZE_8BIT,
                                transfer->data, transfer->size);
    } else {
      ret = HAL_I2C_Mem_Write_IT(hdl, transfer->addr, transfer->reg, I2C_MEMADD_SIZE_8BIT,
                                 transfer->data, transfer->size);
    }
  } else {
    if (transfer->dir == PBL_I2C_READ) {
      ret = HAL_I2C_Master_Receive_IT(hdl, transfer->addr, transfer->data, transfer->size);
    } else {
      ret = HAL_I2C_Master_Transmit_IT(hdl, transfer->addr, transfer->data, transfer->size);
    }
  }

  if (ret != HAL_OK) {
    HAL_I2C_Reset(hdl);
    prv_deepsleep_allow(i2c);
    prv_dma_finish(i2c, false);
    pbl_i2c_bus_event(bus, PBL_I2C_EVENT_ERROR);
  }
}

static void prv_enable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);

  HAL_RCC_EnableModule(i2c->module);
  __HAL_I2C_ENABLE(&i2c->state->hdl);
}

static void prv_disable(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);

  __HAL_I2C_DISABLE(&i2c->state->hdl);
  HAL_RCC_DisableModule(i2c->module);
}

static bool prv_is_busy(const struct pbl_i2c_bus *bus) {
  return HAL_I2C_GetState(&prv_i2c(bus)->state->hdl) != HAL_I2C_STATE_READY;
}

static int prv_init(const struct pbl_i2c_bus *bus) {
  const struct pbl_i2c_sf32lb *i2c = prv_i2c(bus);
  I2C_HandleTypeDef *hdl = &i2c->state->hdl;

  HAL_PIN_Set(i2c->scl_pad, i2c->scl_func, PIN_NOPULL, 1);
  HAL_PIN_Set(i2c->sda_pad, i2c->sda_func, PIN_NOPULL, 1);

  HAL_RCC_EnableModule(i2c->module);
  if (HAL_I2C_Init(hdl) != HAL_OK) {
    return -EIO;
  }

  if (i2c->state->hdma_rx.Instance != NULL) {
    PBL_ASSERTN(dcache_line_size() <= DCACHE_LINE_SIZE_MAX);
    DMA_InitTypeDef *init = &i2c->state->hdma_rx.Init;
    init->Direction = DMA_PERIPH_TO_MEMORY;
    init->PeriphInc = DMA_PINC_DISABLE;
    init->MemInc = DMA_MINC_ENABLE;
    init->PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    init->MemDataAlignment = DMA_MDATAALIGN_BYTE;
    init->Mode = DMA_NORMAL;
    init->Priority = DMA_PRIORITY_HIGH;
    __HAL_LINKDMA(hdl, hdmarx, i2c->state->hdma_rx);
    pbl_irq_enable(i2c->dma_irqn);
  }

  pbl_irq_enable(i2c->irqn);

  return 0;
}

const struct pbl_i2c_bus_ops pbl_i2c_sf32lb_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .is_busy = prv_is_busy,
  .begin_transfer = prv_begin_transfer,
  .start_transfer = prv_start_transfer,
  .abort_transfer = prv_abort_transfer,
};
