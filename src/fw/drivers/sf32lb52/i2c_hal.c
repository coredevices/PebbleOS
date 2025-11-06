/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FreeRTOS.h"
#include "bf0_pin_const.h"
#include "drivers/dma.h"
#include "drivers/gpio.h"
#include "drivers/i2c_definitions.h"
#include "drivers/periph_config.h"
#include "i2c_hal_definitions.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "queue.h"
#include "semphr.h"
#include "system/passert.h"

#define RT_I2C_WR 0x0000
#define RT_I2C_RD (1u << 0)
#define RT_I2C_ADDR_10BIT (1u << 2) /* this is a ten bit chip address */
#define RT_I2C_NO_START (1u << 4)
#define RT_I2C_IGNORE_NACK (1u << 5)
#define RT_I2C_NO_READ_ACK (1u << 6) /* when I2C reading, we do not ACK */

/* read/write specified memory address,
   in this mode, no STOP condition is inserted between memory address and data */
#define RT_I2C_MEM_ACCESS (1u << 7)

typedef struct I2CMsg {
  uint16_t addr;
  uint16_t mem_addr;
  uint16_t mem_addr_size;
  uint16_t flags;
  uint16_t len;
  uint8_t *buf;
} I2CDeviceMsg;

static I2CDeviceMsg msgs[2];
static uint32_t msgs_num;

static void prv_semaphore_give(I2CBusState *bus_state) {
  xSemaphoreGive(bus_state->event_semaphore);
}
static portBASE_TYPE hal_semaphore_give_from_isr(I2CBusState *bus) {
  portBASE_TYPE should_context_switch = pdFALSE;
  (void)xSemaphoreGiveFromISR(bus->event_semaphore, &should_context_switch);
  return should_context_switch;
}

void i2c_irq_handler(I2CBus *bus) {
  I2C_HandleTypeDef *handle = (I2C_HandleTypeDef *)&(bus->hal->hi2c);

  if (handle->XferISR != NULL) {
    handle->XferISR(handle, 0, 0);
  }

  HAL_I2C_StateTypeDef i2c_state = HAL_I2C_GetState(handle);

  if ((i2c_state != HAL_I2C_STATE_BUSY_TX) && (i2c_state != HAL_I2C_STATE_BUSY_RX)) {
    if (i2c_state == HAL_I2C_STATE_READY) {
      i2c_handle_transfer_event(bus, I2CTransferEvent_TransferComplete);
    } else {
      i2c_handle_transfer_event(bus, I2CTransferEvent_Error);
    }
  }
}

void i2c_dma_irq_handler(I2CBus *bus) {
  I2C_HandleTypeDef *handle = (I2C_HandleTypeDef *)&(bus->hal->hi2c);
  if (handle->State == HAL_I2C_STATE_BUSY_TX) {
    HAL_DMA_IRQHandler(handle->hdmatx);
  } else if (handle->State == HAL_I2C_STATE_BUSY_RX) {
    HAL_DMA_IRQHandler(handle->hdmarx);
  } else {
    if (handle->hdmatx != NULL)
      if (HAL_DMA_STATE_BUSY == handle->hdmatx->State) HAL_DMA_IRQHandler(handle->hdmatx);

    if (handle->hdmarx != NULL)
      if (HAL_DMA_STATE_BUSY == handle->hdmarx->State) HAL_DMA_IRQHandler(handle->hdmarx);
  }
}

static HAL_StatusTypeDef i2c_hal_master_xfer(I2CDeviceBusHal *i2c_hal, I2CDeviceMsg msgs[],
                                             uint32_t num) {
  uint32_t index = 0;
  I2CDeviceBusHal *hal = NULL;
  I2CDeviceMsg *msg = NULL;
  HAL_StatusTypeDef status = HAL_ERROR;
  uint16_t mem_addr_type;

  PBL_ASSERTN(i2c_hal != NULL);
  hal = i2c_hal;
  I2C_HandleTypeDef *handle = (I2C_HandleTypeDef *)&hal->hi2c;
  __HAL_I2C_ENABLE(handle);

  for (index = 0; index < num; index++) {
    msg = (I2CDeviceMsg *)&msgs[index];
    if (msg->flags & RT_I2C_MEM_ACCESS) {
      if (8 >= msg->mem_addr_size) {
        mem_addr_type = I2C_MEMADD_SIZE_8BIT;
      } else {
        mem_addr_type = I2C_MEMADD_SIZE_16BIT;
      }
      if (msg->flags & RT_I2C_RD) {
        if (hal->hdma.Instance) {
          HAL_DMA_Init(hal->hi2c.hdmarx);
          mpu_dcache_invalidate(msg->buf, msg->len);
          status = HAL_I2C_Mem_Read_DMA(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                        msg->len);
        } else if (hal->i2c_state->int_enabled) {
          status = HAL_I2C_Mem_Read_IT(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                       msg->len);
        } else {
          status = HAL_I2C_Mem_Read(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                    msg->len, hal->timeout);
        }
      } else {
        if (hal->hdma.Instance) {
          HAL_DMA_Init(hal->hi2c.hdmatx);
          status = HAL_I2C_Mem_Write_DMA(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                         msg->len);
        } else if (hal->i2c_state->int_enabled) {
          status = HAL_I2C_Mem_Write_IT(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                        msg->len);
        } else {
          status = HAL_I2C_Mem_Write(handle, msg->addr, msg->mem_addr, mem_addr_type, msg->buf,
                                     msg->len, hal->timeout);
        }
      }
    } else {
      if (msg->flags & RT_I2C_RD) {
        if (hal->hdma.Instance) {
          HAL_DMA_Init(hal->hi2c.hdmarx);
          mpu_dcache_invalidate(msg->buf, msg->len);
          status = HAL_I2C_Master_Receive_DMA(handle, msg->addr, msg->buf, msg->len);
        } else if (hal->i2c_state->int_enabled) {
          status = HAL_I2C_Master_Receive_IT(handle, msg->addr, msg->buf, msg->len);
        } else {
          status = HAL_I2C_Master_Receive(handle, msg->addr, msg->buf, msg->len, hal->timeout);
        }
      } else {
        if (hal->hdma.Instance) {
          HAL_DMA_Init(hal->hi2c.hdmatx);
          status = HAL_I2C_Master_Transmit_DMA(handle, msg->addr, msg->buf, msg->len);
        } else if (hal->i2c_state->int_enabled) {
          status = HAL_I2C_Master_Transmit_IT(handle, msg->addr, msg->buf, msg->len);
        } else {
          status = HAL_I2C_Master_Transmit(handle, msg->addr, msg->buf, msg->len, hal->timeout);
        }
      }
    }
  }

  return status;
}

void i2c_hal_init_transfer(I2CBus *bus) {
  if (I2CTransferType_SendRegisterAddress == bus->state->transfer.type) {
    if (bus->state->transfer.direction == I2CTransferDirection_Write) {
      msgs[0].addr = bus->state->transfer.device_address;
      msgs[0].mem_addr = bus->state->transfer.register_address;
      msgs[0].mem_addr_size = 8;  // 8bit address
      msgs[0].flags = RT_I2C_WR | RT_I2C_MEM_ACCESS;
      msgs[0].len = bus->state->transfer.size;
      msgs[0].buf = bus->state->transfer.data;
      msgs_num = 1;
    } else {
      msgs[0].addr = bus->state->transfer.device_address;
      msgs[0].mem_addr = bus->state->transfer.register_address;
      msgs[0].mem_addr_size = 8;  // 8bit address
      msgs[0].flags = RT_I2C_RD | RT_I2C_MEM_ACCESS;
      msgs[0].len = bus->state->transfer.size;
      msgs[0].buf = bus->state->transfer.data;
      msgs_num = 1;
    }

  } else {
    if (bus->state->transfer.direction == I2CTransferDirection_Write) {
      msgs[0].addr = bus->state->transfer.device_address;
      msgs[0].flags = RT_I2C_WR;
      msgs[0].len = bus->state->transfer.size;
      msgs[0].buf = bus->state->transfer.data;
      msgs_num = 1;
    } else {
      msgs[0].addr = bus->state->transfer.device_address;
      msgs[0].flags = RT_I2C_RD;
      msgs[0].len = bus->state->transfer.size;
      msgs[0].buf = bus->state->transfer.data;
      msgs_num = 1;
    }
  }
}

void i2c_hal_abort_transfer(I2CBus *bus) {
  struct I2CBusHal *hal = (struct I2CBusHal *)bus->hal;
  HAL_I2C_Reset(&(hal->hi2c));
  PBL_LOG_D(LOG_DOMAIN_I2C, LOG_LEVEL_INFO, "reset and send 9 clks");
  __HAL_I2C_DISABLE(&(hal->hi2c));
}

void i2c_hal_start_transfer(I2CBus *bus) {
  struct I2CBusHal *hal = (struct I2CBusHal *)bus->hal;
  I2C_HandleTypeDef *handle = (I2C_HandleTypeDef *)&hal->hi2c;
  HAL_StatusTypeDef status = i2c_hal_master_xfer(hal, &msgs[0], msgs_num);

  I2CTransferEvent  event = I2CTransferEvent_Error;
  if (HAL_OK != status) {
    event = I2CTransferEvent_Error;
    HAL_I2C_Reset(handle);
    __HAL_I2C_DISABLE(handle);
    bus->state->transfer_event = event;
    prv_semaphore_give(bus->state);
  }
}

int i2c_hal_configure(I2CDeviceBusHal *i2c_hal) {
  HAL_StatusTypeDef ret = HAL_OK;
  PBL_ASSERTN(i2c_hal != NULL);
  I2C_HandleTypeDef *handle = (I2C_HandleTypeDef *)&(i2c_hal->hi2c);

  HAL_RCC_EnableModule(i2c_hal->module);
  ret = HAL_I2C_Init(handle);

  if (ret != HAL_OK) {
    PBL_LOG_D(LOG_DOMAIN_I2C, LOG_LEVEL_ERROR, "I2C [%s] bus_configure fail!",
              i2c_hal->device_name);
    return -1;
  }
  PBL_LOG_D(LOG_DOMAIN_I2C, LOG_LEVEL_INFO, "I2C [%s] bus_configure ok!", i2c_hal->device_name);
  return 0;
}

void i2c_hal_enable(I2CBus *bus) {
  HAL_RCC_EnableModule(bus->hal->module);
}

void i2c_hal_disable(I2CBus *bus) {
  HAL_RCC_DisableModule(bus->hal->module);
}

bool i2c_hal_is_busy(I2CBus *bus) {
  bool ret = false;
  struct I2CBusHal *hal = (struct I2CBusHal *)bus->hal;
  if (HAL_I2C_GetState(&(hal->hi2c)) != HAL_I2C_STATE_READY) ret = true;
  return ret;
}

static int i2c_hal_hw_init(struct I2CBusHal *i2c_hal) {
  int ret = 0;
  struct dma_config dma_rtx_config;

  if (i2c_hal->hdma.Instance != NULL) {
    __HAL_LINKDMA(&(i2c_hal->hi2c), hdmarx, i2c_hal->hdma);
    __HAL_LINKDMA(&(i2c_hal->hi2c), hdmatx, i2c_hal->hdma);
    dma_rtx_config.Instance = i2c_hal->hdma.Instance;
    dma_rtx_config.request = i2c_hal->hdma.Init.Request;
    HAL_I2C_DMA_Init(&(i2c_hal->hi2c), &dma_rtx_config, &dma_rtx_config);
  }

  ret = i2c_hal_configure(i2c_hal);

  if (i2c_hal->i2c_state->int_enabled) {
    HAL_NVIC_SetPriority(i2c_hal->irqn, i2c_hal->irq_priority, 0);
    NVIC_EnableIRQ(i2c_hal->irqn);
  }

  if (ret < 0) {
    return ret;
  }
  return ret;
}

void i2c_hal_init(I2CBus *bus) {
  int ret = 0;
  PBL_ASSERTN(bus != NULL);
  ret = i2c_hal_hw_init((struct I2CBusHal *)bus->hal);
  if (ret < 0) {
    PBL_LOG_D(LOG_DOMAIN_I2C, LOG_LEVEL_ERROR, "I2C [%s] hw init fail!", bus->hal->device_name);
  } else {
    PBL_LOG_D(LOG_DOMAIN_I2C, LOG_LEVEL_INFO, "I2C [%s] hw init ok!", bus->hal->device_name);
  }

  return;
}

void i2c_hal_pins_set_gpio(I2CBus *bus) {}

void i2c_hal_pins_set_i2c(I2CBus *bus) {
  HAL_PIN_Set(bus->hal->scl.pad, bus->hal->scl.func, bus->hal->scl.flags, 1);
  HAL_PIN_Set(bus->hal->sda.pad, bus->hal->sda.func, bus->hal->sda.flags, 1);
}
