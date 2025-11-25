/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "i2c_hal_definitions.h"

#include "drivers/i2c_definitions.h"
#include "drivers/i2c_hal.h"
#include "system/passert.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "bf0_hal.h"

void i2c_irq_handler(I2CBus *bus) {
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &hal->state->hdl;
  HAL_I2C_StateTypeDef state;
  I2CTransferEvent event;
  portBASE_TYPE woken;

  (void)hdl->XferISR(hdl, 0, 0);

  state = HAL_I2C_GetState(hdl);
  if ((state == HAL_I2C_STATE_BUSY_TX) || (state == HAL_I2C_STATE_BUSY_RX)) {
    return;
  } else if (state == HAL_I2C_STATE_READY) {
    event = I2CTransferEvent_TransferComplete;
  } else {
    event = I2CTransferEvent_Error;
  }

  woken = i2c_handle_transfer_event(bus, event);
  portEND_SWITCHING_ISR(woken);
}

void i2c_hal_init_transfer(I2CBus *bus) {}

void i2c_hal_abort_transfer(I2CBus *bus) {
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &hal->state->hdl;

  HAL_I2C_Reset(hdl);
}

void i2c_hal_start_transfer(I2CBus *bus) {
  HAL_StatusTypeDef ret;
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &hal->state->hdl;
  I2CTransfer *transfer = &bus->state->transfer;

  if (transfer->type == I2CTransferType_SendRegisterAddress) {
    if (transfer->direction == I2CTransferDirection_Read) {
      ret = HAL_I2C_Mem_Read_IT(hdl, transfer->device_address, transfer->register_address,
                                I2C_MEMADD_SIZE_8BIT, transfer->data, transfer->size);
    } else {
      ret = HAL_I2C_Mem_Write_IT(hdl, transfer->device_address, transfer->register_address,
                                 I2C_MEMADD_SIZE_8BIT, transfer->data, transfer->size);
    }
  } else {
    if (transfer->direction == I2CTransferDirection_Read) {
      ret =
          HAL_I2C_Master_Receive_IT(hdl, transfer->device_address, transfer->data, transfer->size);
    } else {
      ret =
          HAL_I2C_Master_Transmit_IT(hdl, transfer->device_address, transfer->data, transfer->size);
    }
  }

  if (ret != HAL_OK) {
    HAL_I2C_Reset(hdl);
    bus->state->transfer_event = I2CTransferEvent_Error;
    xSemaphoreGive(bus->state->event_semaphore);
  }
}

void i2c_hal_enable(I2CBus *bus) {
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &bus->hal->state->hdl;

  HAL_RCC_EnableModule(hal->module);
  __HAL_I2C_ENABLE(hdl);
}

void i2c_hal_disable(I2CBus *bus) {
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &bus->hal->state->hdl;

  __HAL_I2C_DISABLE(hdl);
  HAL_RCC_DisableModule(hal->module);
}

bool i2c_hal_is_busy(I2CBus *bus) {
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &hal->state->hdl;

  return HAL_I2C_GetState(hdl) != HAL_I2C_STATE_READY;
}

void i2c_hal_init(I2CBus *bus) {
  HAL_StatusTypeDef ret;
  I2CBusHal *hal = bus->hal;
  I2C_HandleTypeDef *hdl = &hal->state->hdl;

  HAL_PIN_Set(hal->scl.pad, hal->scl.func, hal->scl.flags, 1);
  HAL_PIN_Set(hal->sda.pad, hal->sda.func, hal->sda.flags, 1);

  HAL_RCC_EnableModule(hal->module);
  ret = HAL_I2C_Init(hdl);
  PBL_ASSERTN(ret == HAL_OK);

  HAL_NVIC_SetPriority(hal->irqn, hal->irq_priority, 0);
  NVIC_EnableIRQ(hal->irqn);
}
