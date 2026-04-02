/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hal.h"
#include "definitions.h"
#include "nrf5.h"

#include "system/passert.h"

#include "drivers/periph_config.h"

#include <string.h>
#include "FreeRTOS.h"

#include <nrfx.h>

#define I2C_IRQ_PRIORITY (0xc)
#define I2C_NORMAL_MODE_CLOCK_SPEED_MAX   (100000)
#define I2C_READ_WRITE_BIT    (0x01)

static void prv_twim_evt_handler(nrfx_twim_evt_t const *evt, void *ctx) {
  I2CBus *bus = (I2CBus *) ctx;
  bool success = evt->type == NRFX_TWIM_EVT_DONE;
  I2CTransferEvent event = success ? I2CTransferEvent_TransferComplete : I2CTransferEvent_Error;
  bool should_csw = i2c_handle_transfer_event(bus, event);
  portEND_SWITCHING_ISR(should_csw);
}

static void prv_twim_init(I2CBus *bus) {
  nrfx_twim_config_t config = NRFX_TWIM_DEFAULT_CONFIG(
    bus->scl_gpio.gpio_pin, bus->sda_gpio.gpio_pin);
  config.frequency = bus->hal->frequency;
  config.hold_bus_uninit = true;
  
  nrfx_err_t err = nrfx_twim_init(&bus->hal->twim, &config, prv_twim_evt_handler, (void *)bus);
  PBL_ASSERTN(err == NRFX_SUCCESS);
}

void i2c_hal_init(I2CBus *bus) {
  prv_twim_init(bus); 
  nrfx_twim_uninit(&bus->hal->twim);
}

void i2c_hal_enable(I2CBus *bus) {
  prv_twim_init(bus); 
  nrfx_twim_enable(&bus->hal->twim);
}

void i2c_hal_disable(I2CBus *bus) {
  nrfx_twim_disable(&bus->hal->twim);
  nrfx_twim_uninit(&bus->hal->twim);
}

bool i2c_hal_is_busy(I2CBus *bus) {
  return nrfx_twim_is_busy(&bus->hal->twim);
}

void i2c_hal_abort_transfer(I2CBus *bus) {
  nrfx_twim_disable(&bus->hal->twim);
  nrfx_twim_enable(&bus->hal->twim);
}

void i2c_hal_init_transfer(I2CBus *bus) {
}

// Buffer for combining register address + write data into a single TX transfer.
// TXTX (two-part TX) on nRF52840 can fail to transmit the secondary buffer
// correctly due to TWIM peripheral errata. Using a single TX avoids this.
#define I2C_WRITE_BUF_MAX 32
static uint8_t s_write_buf[I2C_WRITE_BUF_MAX];

void i2c_hal_start_transfer(I2CBus *bus) {
  nrfx_twim_xfer_desc_t desc;

  desc.address = bus->state->transfer.device_address >> 1;
  if (bus->state->transfer.type == I2CTransferType_SendRegisterAddress) {
    if (bus->state->transfer.direction == I2CTransferDirection_Read) {
      desc.type = NRFX_TWIM_XFER_TXRX;
      desc.primary_length = 1;
      desc.p_primary_buf = &bus->state->transfer.register_address;
      desc.secondary_length = bus->state->transfer.size;
      desc.p_secondary_buf = bus->state->transfer.data;
    } else {
      // Combine register address + data into one TX to avoid TXTX errata
      PBL_ASSERTN(bus->state->transfer.size + 1 <= I2C_WRITE_BUF_MAX);
      s_write_buf[0] = bus->state->transfer.register_address;
      memcpy(&s_write_buf[1], bus->state->transfer.data, bus->state->transfer.size);
      desc.type = NRFX_TWIM_XFER_TX;
      desc.primary_length = bus->state->transfer.size + 1;
      desc.p_primary_buf = s_write_buf;
      desc.secondary_length = 0;
      desc.p_secondary_buf = NULL;
    }
  } else {
    if (bus->state->transfer.direction == I2CTransferDirection_Read) {
      desc.type = NRFX_TWIM_XFER_RX;
    } else {
      desc.type = NRFX_TWIM_XFER_TX;
    }
    desc.primary_length = bus->state->transfer.size;
    desc.p_primary_buf = bus->state->transfer.data;
    desc.secondary_length = 0;
    desc.p_secondary_buf = NULL;
  }

  nrfx_err_t rv = nrfx_twim_xfer(&bus->hal->twim, &desc, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}
