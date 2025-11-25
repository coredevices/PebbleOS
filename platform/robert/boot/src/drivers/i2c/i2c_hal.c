/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "i2c_hal.h"
#include "i2c_definitions.h"
#include "i2c_hal_definitions.h"

#include "drivers/periph_config.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/attributes.h"

#include "stm32f7xx.h"

#define I2C_IRQ_PRIORITY (0xc)
#define I2C_NORMAL_MODE_CLOCK_SPEED_MAX     (100000)
#define I2C_FAST_MODE_CLOCK_SPEED_MAX       (400000)
#define I2C_FAST_MODE_PLUS_CLOCK_SPEED_MAX  (1000000)

#define TIMINGR_MASK_PRESC  (0x0F)
#define TIMINGR_MASK_SCLH   (0xFF)
#define TIMINGR_MASK_SCLL   (0xFF)

#define CR1_CLEAR_MASK (0x00CFE0FF)

#define CR2_CLEAR_MASK (0x07FF7FFF)
#define CR2_NBYTES_OFFSET (16)
#define CR2_TRANSFER_SETUP_MASK (I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RELOAD | \
                                 I2C_CR2_AUTOEND | I2C_CR2_RD_WRN | I2C_CR2_START | \
                                 I2C_CR2_STOP)

typedef union PACKED TIMINGR {
  struct {
    int32_t SCLL:8;
    int32_t SCLH:8;
    int32_t SDADEL:4;
    int32_t SCLDEL:4;
    int32_t reserved:4;
    int32_t PRESC:4;
  };
  int32_t reg;
} TIMINGR;

static void prv_i2c_deinit(I2CBus *bus) {
  // Reset the clock to the peripheral
  RCC_APB1PeriphResetCmd(bus->hal->clock_ctrl, ENABLE);
  RCC_APB1PeriphResetCmd(bus->hal->clock_ctrl, DISABLE);
}

void i2c_hal_init(I2CBus *bus) {
  NVIC_SetPriority(bus->hal->ev_irq_channel, I2C_IRQ_PRIORITY);
  NVIC_SetPriority(bus->hal->er_irq_channel, I2C_IRQ_PRIORITY);
  NVIC_EnableIRQ(bus->hal->ev_irq_channel);
  NVIC_EnableIRQ(bus->hal->er_irq_channel);
  prv_i2c_deinit(bus);
}

static void prv_i2c_init(I2C_TypeDef *i2c, TIMINGR timingr) {
  // Soft reset of the state machine and status bits by disabling the peripheral.
  // Note: PE must be low for 3 APB cycles after this is done for the reset to be successful
  i2c->CR1 &= ~I2C_CR1_PE;

  i2c->CR1 &= ~CR1_CLEAR_MASK;

  // Set the timing register
  i2c->TIMINGR = timingr.reg;

  // I2C only used as a master; disable slave address acknowledgement
  i2c->OAR1 = 0;
  i2c->OAR2 = 0;

  // Enable i2c Peripheral; clear any configured interrupt bits; use analog filter
  i2c->CR1 |= I2C_CR1_PE;

  // Clear CR2, making it ready for the next transaction
  i2c->CR2 &= ~CR2_CLEAR_MASK;
}

void i2c_hal_enable(I2CBus *bus) {
  // We don't need to support Fast Mode Plus yet, so make sure the desired clock speed is less than
  // the maximum Fast Mode clock speed.
  // When Fast Mode support is added the duty-cycle settings will probably have to be re-thought.
  PBL_ASSERT(bus->hal->clock_speed <= I2C_FAST_MODE_CLOCK_SPEED_MAX,
             "Fast Mode Plus not yet supported");

  uint32_t duty_cycle_low = 1;
  uint32_t duty_cycle_high = 1;
  if (bus->hal->clock_speed > I2C_NORMAL_MODE_CLOCK_SPEED_MAX) {  // Fast mode
    if (bus->hal->duty_cycle == I2CDutyCycle_16_9) {
      duty_cycle_low = 16;
      duty_cycle_high = 9;
    } else if (bus->hal->duty_cycle == I2CDutyCycle_2) {
      duty_cycle_low = 2;
      duty_cycle_high = 1;
    } else {
      WTF; // It might be possible to encode a duty cycle differently from the legacy I2C, if it's
           // ever necessary. Currently it's not, so just maintain the previous implementation
    }
  }

  RCC_ClocksTypeDef rcc_clocks;
  RCC_GetClocksFreq(&rcc_clocks);

  uint32_t prescaler = rcc_clocks.PCLK1_Frequency /
      (bus->hal->clock_speed * (duty_cycle_low + duty_cycle_high));
  if ((rcc_clocks.PCLK1_Frequency %
      (bus->hal->clock_speed * (duty_cycle_low + duty_cycle_high))) == 0) {
    // Prescaler is PRESC + 1. This subtracts one so that exact dividers are correct, but if there
    // is an integer remainder, the prescaler will ensure that the clock frequency is within spec.
    prescaler -= 1;
  }
  // Make sure all the values fit in their corresponding fields
  PBL_ASSERTN((duty_cycle_low <= TIMINGR_MASK_SCLL) &&
              (duty_cycle_high <= TIMINGR_MASK_SCLH) &&
              (prescaler <= TIMINGR_MASK_PRESC));

  periph_config_enable(bus->hal->i2c, bus->hal->clock_ctrl);

  // We currently don't need to worry about the other TIMINGR fields (they come out to 0), but might
  // need to revisit this if we ever need FM+ speeds.
  TIMINGR timingr = {
    .PRESC = prescaler,
    .SCLH = duty_cycle_high - 1, // Duty cycle high is SCLH + 1
    .SCLL = duty_cycle_low - 1,  // Duty cycle low is SCLL + 1
  };
  prv_i2c_init(bus->hal->i2c, timingr);
}

void i2c_hal_disable(I2CBus *bus) {
  periph_config_disable(bus->hal->i2c, bus->hal->clock_ctrl);
  prv_i2c_deinit(bus);
}

bool i2c_hal_is_busy(I2CBus *bus) {
  return ((bus->hal->i2c->ISR & I2C_ISR_BUSY) != 0);
}

static void prv_disable_all_interrupts(I2CBus *bus) {
  bus->hal->i2c->CR1 &= ~(I2C_CR1_TXIE |
                          I2C_CR1_RXIE |
                          I2C_CR1_TCIE |
                          I2C_CR1_NACKIE |
                          I2C_CR1_ERRIE);
}

void i2c_hal_abort_transfer(I2CBus *bus) {
  // Disable all interrupts on the bus
  prv_disable_all_interrupts(bus);
  // Generate a stop condition
  bus->hal->i2c->CR2 |= I2C_CR2_STOP;
}

void i2c_hal_init_transfer(I2CBus *bus) {
  I2CTransfer *transfer = &bus->state->transfer;

  if (transfer->type == I2CTransferType_SendRegisterAddress) {
    transfer->state = I2CTransferState_WriteRegAddress;
  } else {
    if (transfer->direction == I2CTransferDirection_Read) {
      transfer->state = I2CTransferState_ReadData;
    } else {
      transfer->state = I2CTransferState_WriteData;
    }
  }
}

static void prv_enable_interrupts(I2CBus *bus) {
  bus->hal->i2c->CR1 |= I2C_CR1_ERRIE |  // Enable error interrupt
                        I2C_CR1_NACKIE | // Enable NACK interrupt
                        I2C_CR1_TCIE | // Enable transfer complete interrupt
                        I2C_CR1_TXIE; // Enable transmit interrupt
  if (bus->state->transfer.direction == I2CTransferDirection_Read) {
    bus->hal->i2c->CR1 |= I2C_CR1_RXIE;  // Enable receive interrupt
  }
}

static void prv_resume_transfer(I2CBus *bus, bool generate_start) {
  const I2CTransfer *transfer = &bus->state->transfer;
  uint32_t cr2_value = transfer->device_address & I2C_CR2_SADD;

  if ((transfer->direction == I2CTransferDirection_Read) &&
      (transfer->state != I2CTransferState_WriteRegAddress)) {
    cr2_value |= I2C_CR2_RD_WRN;
  }

  const uint32_t remaining = bus->state->transfer.size - bus->state->transfer.idx;
  if (remaining > UINT8_MAX) {
    cr2_value |= I2C_CR2_RELOAD;
    cr2_value |= I2C_CR2_NBYTES;
  } else {
    cr2_value |= (remaining << CR2_NBYTES_OFFSET) & I2C_CR2_NBYTES;
  }

  if (generate_start) {
    cr2_value |= I2C_CR2_START;
  }

  bus->hal->i2c->CR2 = cr2_value;
}

void i2c_hal_start_transfer(I2CBus *bus) {
  prv_enable_interrupts(bus);
  if (bus->state->transfer.state == I2CTransferState_WriteRegAddress) {
    // For writes, we'll reload with the payload once we send the address. Otherwise, we'd need to
    // send a repeated start, which we don't want to do.
    const bool reload = bus->state->transfer.direction == I2CTransferDirection_Write;
    bus->hal->i2c->CR2 = (bus->state->transfer.device_address & I2C_CR2_SADD) |
                         (1 << CR2_NBYTES_OFFSET) |
                         (reload ? I2C_CR2_RELOAD : 0) |
                         I2C_CR2_START;
  } else {
    prv_resume_transfer(bus, true /* generate_start */);
  }
}

/*------------------------INTERRUPT FUNCTIONS--------------------------*/
static void prv_end_transfer_irq(I2CBus *bus, I2CTransferEvent event) {
  prv_disable_all_interrupts(bus);

  // Generate stop condition
  bus->hal->i2c->CR2 |= I2C_CR2_STOP;
  bus->state->transfer.state = I2CTransferState_Complete;

  i2c_handle_transfer_event(bus, event);
}

//! Handle an IRQ event on the specified \a bus
static void prv_event_irq_handler(I2CBus *bus) {
  I2C_TypeDef *i2c = bus->hal->i2c;
  I2CTransfer *transfer = &bus->state->transfer;
  switch (transfer->state) {
    case I2CTransferState_WriteRegAddress:
      if ((i2c->ISR & I2C_ISR_TXIS) != 0) {
        i2c->TXDR = transfer->register_address;
      }
      if ((transfer->direction == I2CTransferDirection_Read) && (i2c->ISR & I2C_ISR_TC)) {
        // done writing the register address for a read request - start a read request
        transfer->state = I2CTransferState_ReadData;
        prv_resume_transfer(bus, true /* generate_start */);
      } else if ((transfer->direction == I2CTransferDirection_Write) && (i2c->ISR & I2C_ISR_TCR)) {
        // done writing the register address for a write request - "reload" the write payload
        transfer->state = I2CTransferState_WriteData;
        prv_resume_transfer(bus, false /* !generate_start */);
      }
      if ((i2c->ISR & I2C_ISR_NACKF) != 0) {
        i2c->ICR |= I2C_ICR_NACKCF;
        i2c_handle_transfer_event(bus, I2CTransferEvent_NackReceived);
        return;
      }
      break;

    case I2CTransferState_ReadData:
      if ((i2c->ISR & I2C_ISR_RXNE) != 0) {
        transfer->data[transfer->idx++] = i2c->RXDR;
      }
      if ((i2c->ISR & I2C_ISR_TCR) != 0) {
        prv_resume_transfer(bus, false /* !generate_start */);
      }
      if ((i2c->ISR & I2C_ISR_TC) != 0) {
        prv_end_transfer_irq(bus, I2CTransferEvent_TransferComplete);
        return;
      }
      break;

    case I2CTransferState_WriteData:
      if ((i2c->ISR & I2C_ISR_TXIS) != 0) {
        i2c->TXDR = transfer->data[transfer->idx++];
      }
      if ((i2c->ISR & I2C_ISR_NACKF) != 0) {
        i2c->ICR |= I2C_ICR_NACKCF;
        return i2c_handle_transfer_event(bus, I2CTransferEvent_NackReceived);
      }
      if ((i2c->ISR & I2C_ISR_TCR) != 0) {
        prv_resume_transfer(bus, false /* !generate_start */);
      }
      if ((i2c->ISR & I2C_ISR_TC) != 0) {
        prv_end_transfer_irq(bus, I2CTransferEvent_TransferComplete);
        return;
      }
      break;

    case I2CTransferState_Complete:
      if (i2c->ISR & I2C_ISR_TXE) {
        // We seem to get a spurious interrupt after the last byte is sent
        // There is no bit to specifically disable this interrupt and the interrupt may have already
        // been pended when we would disable it, so just handle it silently.
        break;
      }
      // Fallthrough

    // These extra states were defined for the F4 implementation but are not necessary for the F7,
    // because the interrupt scheme is a lot nicer.
    case I2CTransferState_RepeatStart:
    case I2CTransferState_EndWrite:
    case I2CTransferState_WaitForData:
    case I2CTransferState_WriteAddressRx:
    case I2CTransferState_WriteAddressTx:
    default:
      WTF;
  }
}

static void prv_error_irq_handler(I2CBus *bus) {
  I2C_TypeDef *i2c = bus->hal->i2c;
  if ((i2c->ISR & I2C_ISR_BERR) != 0) {
    i2c->ICR |= I2C_ICR_BERRCF;
  }
  if ((i2c->ISR & I2C_ISR_OVR) != 0) {
    i2c->ICR |= I2C_ICR_OVRCF;
  }
  if ((i2c->ISR & I2C_ISR_ARLO) != 0) {
    i2c->ICR |= I2C_ICR_ARLOCF;
  }
  prv_end_transfer_irq(bus, I2CTransferEvent_Error);
}

void i2c_hal_event_irq_handler(I2CBus *bus) {
  prv_event_irq_handler(bus);
}

void i2c_hal_error_irq_handler(I2CBus *bus) {
  prv_error_irq_handler(bus);
}
