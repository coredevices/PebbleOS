/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/i2c_hal.h"
#include "drivers/i2c_definitions.h"
#include "drivers/stm32f2/i2c_hal_definitions.h"
#include "system/passert.h"

#include "drivers/periph_config.h"
#include "FreeRTOS.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#include <mcu.h>

#define I2C_IRQ_PRIORITY (0xc)
#define I2C_NORMAL_MODE_CLOCK_SPEED_MAX   (100000)
#define I2C_READ_WRITE_BIT    (0x01)


static uint32_t s_guard_events[] = {
  I2C_EVENT_MASTER_MODE_SELECT,
  I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED,
  I2C_EVENT_MASTER_BYTE_TRANSMITTED,
  I2C_EVENT_MASTER_MODE_SELECT,
  I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED,
  I2C_EVENT_MASTER_BYTE_RECEIVED,
  I2C_EVENT_MASTER_BYTE_TRANSMITTING,
  I2C_EVENT_MASTER_BYTE_TRANSMITTED,
};

void i2c_hal_init(I2CBus *bus) {
  NVIC_SetPriority(bus->hal->ev_irq_channel, I2C_IRQ_PRIORITY);
  NVIC_SetPriority(bus->hal->er_irq_channel, I2C_IRQ_PRIORITY);
  NVIC_EnableIRQ(bus->hal->ev_irq_channel);
  NVIC_EnableIRQ(bus->hal->er_irq_channel);
  I2C_DeInit(bus->hal->i2c);
}

static uint32_t prv_get_apb1_frequency(void) {
  RCC_ClocksTypeDef rcc_clocks;
  RCC_GetClocksFreq(&rcc_clocks);
  return rcc_clocks.PCLK1_Frequency;
}

static const int DUTY_CYCLE_DIVIDERS[] = {
  [I2CDutyCycle_16_9] = 25,
  [I2CDutyCycle_2] = 3
};

static uint32_t prv_prescalar_to_frequency(I2CDutyCycle duty_cycle, uint32_t prescalar) {
  const uint32_t pclk1 = prv_get_apb1_frequency();
  return pclk1 / (prescalar * DUTY_CYCLE_DIVIDERS[duty_cycle]);
}

//! @return A prescalar that will result in a frequency that's close to but not greater than
//! desired_maximum_frequency.
static uint32_t prv_frequency_to_prescalar(I2CDutyCycle duty_cycle,
                                           uint32_t desired_maximum_frequency) {
  const uint32_t pclk1 = prv_get_apb1_frequency();

  uint32_t prescalar =
      pclk1 / (desired_maximum_frequency * DUTY_CYCLE_DIVIDERS[duty_cycle]);

  // Check to see what frequency our calculated prescalar is actually going to give us. If the
  // numbers don't divide evenly, that means we'll have a prescalar that's too low, and will end
  // up giving us a speed that's faster than we wanted. Add one to the prescalar in this case
  // to make sure we stay within spec.
  const uint32_t remainder =
      pclk1 % (desired_maximum_frequency * DUTY_CYCLE_DIVIDERS[duty_cycle]);
  if (remainder != 0) {
    prescalar += 1;
  }

  return prescalar;
}

void i2c_hal_enable(I2CBus *bus) {
  periph_config_enable(bus->hal->i2c, bus->hal->clock_ctrl);

  I2C_InitTypeDef I2C_init_struct;
  I2C_StructInit(&I2C_init_struct);

  if (bus->hal->clock_speed > I2C_NORMAL_MODE_CLOCK_SPEED_MAX) {  // Fast mode
    switch (bus->hal->duty_cycle) {
    case I2CDutyCycle_16_9:
      I2C_init_struct.I2C_DutyCycle = I2C_DutyCycle_16_9;
      break;
    case I2CDutyCycle_2:
      I2C_init_struct.I2C_DutyCycle = I2C_DutyCycle_2;
      break;
    default:
      WTF;
    }
  }

  // Calculate the prescalar we're going to end up using to get as close as possible to
  // bus->hal->clock_speed without going over.
  const uint32_t prescalar = prv_frequency_to_prescalar(bus->hal->duty_cycle,
                                                        bus->hal->clock_speed);

  // Convert it back to a frequency since the I2C_Init function wants a frequency, not a raw
  // prescalar value.
  const uint32_t frequency = prv_prescalar_to_frequency(bus->hal->duty_cycle,
                                                        prescalar);
  I2C_init_struct.I2C_ClockSpeed = frequency;

  I2C_init_struct.I2C_Ack = I2C_Ack_Enable;

  I2C_Init(bus->hal->i2c, &I2C_init_struct);
  I2C_Cmd(bus->hal->i2c, ENABLE);
}

void i2c_hal_disable(I2CBus *bus) {
  periph_config_disable(bus->hal->i2c, bus->hal->clock_ctrl);
  I2C_DeInit(bus->hal->i2c);
}

bool i2c_hal_is_busy(I2CBus *bus) {
  return ((bus->hal->i2c->SR2 & I2C_SR2_BUSY) != 0);
}

void prv_disable_all_interrupts(I2CBus *bus) {
  bus->hal->i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
}

void i2c_hal_abort_transfer(I2CBus *bus) {
  // Disable all interrupts on the bus
  prv_disable_all_interrupts(bus);
  // Generate a stop condition
  bus->hal->i2c->CR1 |= I2C_CR1_STOP;
}

void i2c_hal_init_transfer(I2CBus *bus) {
  // Enable Acks
  bus->hal->i2c->CR1 |= I2C_CR1_ACK;
  bus->state->transfer.state = I2CTransferState_WriteAddressTx;
}

void i2c_hal_start_transfer(I2CBus *bus) {
  // Generate start event
  bus->hal->i2c->CR1 |= I2C_CR1_START;
  // Enable event and error interrupts
  bus->hal->i2c->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
}

/*------------------------INTERRUPT FUNCTIONS--------------------------*/

//! End a transfer and disable further interrupts
//! Only call from interrupt functions
static portBASE_TYPE prv_end_transfer_irq(I2CBus *bus, bool result) {
  prv_disable_all_interrupts(bus);

  // Generate stop condition
  bus->hal->i2c->CR1 |= I2C_CR1_STOP;
  bus->state->transfer.state = I2CTransferState_Complete;

  I2CTransferEvent event = result ? I2CTransferEvent_TransferComplete : I2CTransferEvent_Error;
  return i2c_handle_transfer_event(bus, event);
}

//! Pause a transfer, disabling interrupts during the pause
//! Only call from interrupt functions
static portBASE_TYPE prv_pause_transfer_irq(I2CBus *bus) {
  prv_disable_all_interrupts(bus);
  return i2c_handle_transfer_event(bus, I2CTransferEvent_NackReceived);
}

//! Handle an IRQ event on the specified \a bus
static portBASE_TYPE prv_event_irq_handler(I2CBus *bus) {
  I2C_TypeDef *i2c = bus->hal->i2c;
  I2CTransfer *transfer = &bus->state->transfer;

  if (transfer->state == I2CTransferState_Complete) {
    // Disable interrupts if spurious interrupt received
    i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);
    return pdFALSE;
  }

  // Check that the expected event occurred
  if (I2C_CheckEvent(i2c, s_guard_events[transfer->state]) == ERROR) {
    // Ignore interrupt - A spurious byte transmitted event as well as an interrupt with no
    // discernible event associated with it occur after repeat start events are generated
    return pdFALSE;
  }
  portBASE_TYPE should_context_switch = pdFALSE;

  switch (transfer->state) {
    case I2CTransferState_WriteAddressTx:
      if (transfer->type == I2CTransferType_SendRegisterAddress) {
        // Write the I2C bus address to select it in write mode.
        i2c->DR = transfer->device_address & ~I2C_READ_WRITE_BIT;
        transfer->state = I2CTransferState_WriteRegAddress;
      } else {
        if (transfer->direction == I2CTransferDirection_Read) {
          // Write the I2C bus address to select it in read mode.
          i2c->DR = transfer->device_address | I2C_READ_WRITE_BIT;
          transfer->state = I2CTransferState_WaitForData;
        } else {
          // Write the I2C bus address to select it in write mode.
          i2c->DR = transfer->device_address & ~I2C_READ_WRITE_BIT;
          transfer->state = I2CTransferState_WriteData;
        }
      }
      break;

    case I2CTransferState_WriteRegAddress:
      // Write the register address
      i2c->DR = transfer->register_address;

      if (transfer->direction == I2CTransferDirection_Read) {
        transfer->state = I2CTransferState_RepeatStart;
      } else {
        // Enable TXE interrupt for writing
        i2c->CR2 |= I2C_CR2_ITBUFEN;
        transfer->state = I2CTransferState_WriteData;
      }
      break;

    case I2CTransferState_RepeatStart:
      // Generate a repeat start
      i2c->CR1 |= I2C_CR1_START;
      transfer->state = I2CTransferState_WriteAddressRx;
      break;

    case I2CTransferState_WriteAddressRx:
      // Write the I2C bus address again, but this time in read mode.
      i2c->DR = transfer->device_address | I2C_READ_WRITE_BIT;
      if (transfer->size == 1) {
        // Last byte, we want to NACK this one to tell the slave to stop sending us bytes.
        i2c->CR1 &= ~I2C_CR1_ACK;
      }
      transfer->state = I2CTransferState_WaitForData;
      break;

    case I2CTransferState_WaitForData:
      // This state just ensures that the transition to receive mode event happened

      // Enable RXNE interrupt for writing
      i2c->CR2 |= I2C_CR2_ITBUFEN;
      transfer->state = I2CTransferState_ReadData;
      break;

    case I2CTransferState_ReadData:
      transfer->data[transfer->idx] = i2c->DR;
      transfer->idx++;

      if (transfer->idx + 1 == transfer->size) {
        // Last byte, we want to NACK this one to tell the slave to stop sending us bytes.
        i2c->CR1 &= ~I2C_CR1_ACK;
      }
      else if (transfer->idx == transfer->size) {
        // End transfer after all bytes have been received
        i2c->CR2 &= ~I2C_CR2_ITBUFEN;
        should_context_switch = prv_end_transfer_irq(bus, true);
        break;
      }

      break;

    case I2CTransferState_WriteData:
      i2c->DR = transfer->data[transfer->idx];
      transfer->idx++;
      if (transfer->idx == transfer->size) {
        i2c->CR2 &= ~I2C_CR2_ITBUFEN;
        transfer->state = I2CTransferState_EndWrite;
        break;
      }
      break;

    case I2CTransferState_EndWrite:
      // End transfer after all bytes have been sent
      should_context_switch = prv_end_transfer_irq(bus, true);
      break;

    default:
      // Should never reach here (state machine logic broken)
      WTF;
      break;
  }

  return should_context_switch;
}

//! Handle error interrupt on the specified \a bus
static portBASE_TYPE prv_error_irq_handler(I2CBus *bus) {
  I2C_TypeDef *i2c = bus->hal->i2c;
  I2CTransfer *transfer = &bus->state->transfer;

  if (transfer->state == I2CTransferState_Complete) {
    // Disable interrupts if spurious interrupt received
    i2c->CR2 &= ~I2C_CR2_ITERREN;
    return pdFALSE;
  }

  // Data overrun and bus errors can only really be handled by terminating the transfer and
  // trying to recover the bus to an idle state. Each error will be logged. In each case a stop
  // condition will be sent and then we will wait on the busy flag to clear (if it doesn't,
  // a soft reset of the bus will be performed (handled in wait I2C_do_transfer).

  if ((i2c->SR1 & I2C_SR1_OVR) != 0) {
    // Data overrun
    i2c->SR1 &= ~I2C_SR1_OVR;

    I2C_DEBUG("Data overrun during I2C transaction; Bus: %s", bus->name);
  }
  if ((i2c->SR1 & I2C_SR1_BERR) != 0) {
    i2c->SR1 &= ~I2C_SR1_BERR;

    // Bus error: invalid start or stop condition detected
    I2C_DEBUG("Bus error detected during I2C transaction; Bus: %s", bus->name);
  }
  if ((i2c->SR1 & I2C_SR1_AF) != 0) {
    i2c->SR1 &= ~I2C_SR1_AF;

    // NACK received.
    //
    // The MFI chip will cause NACK errors during read operations after writing a start bit (first
    // start or repeat start indicating that it is busy. The transfer must be paused and the start
    // condition sent again after a delay and the state machine set back a step.
    //
    // If the NACK is received after any other action log an error and abort the transfer

    if (transfer->state == I2CTransferState_WaitForData) {
      transfer->state = I2CTransferState_WriteAddressRx;
      return prv_pause_transfer_irq(bus);
    }
    else if (transfer->state == I2CTransferState_WriteRegAddress) {
      transfer->state = I2CTransferState_WriteAddressTx;
      return prv_pause_transfer_irq(bus);
    }
    else {
      I2C_DEBUG("NACK received during I2C transfer; Bus: %s", bus->name);
    }
  }

  return prv_end_transfer_irq(bus, false);
}

void i2c_hal_event_irq_handler(I2CBus *bus) {
  portEND_SWITCHING_ISR(prv_event_irq_handler(bus));
}

void i2c_hal_error_irq_handler(I2CBus *bus) {
  portEND_SWITCHING_ISR(prv_error_irq_handler(bus));
}
