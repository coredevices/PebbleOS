#include "drivers/i2c_hal.h"
#include "drivers/i2c_definitions.h"
#include "drivers/nrf5/i2c_hal_definitions.h"
#include "system/passert.h"

#include "drivers/periph_config.h"
#include "FreeRTOS.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

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

void i2c_hal_start_transfer(I2CBus *bus) {
  nrfx_twim_xfer_desc_t desc;
  
  desc.address = bus->state->transfer.device_address >> 1;
  if (bus->state->transfer.type == I2CTransferType_SendRegisterAddress) {
    if (bus->state->transfer.direction == I2CTransferDirection_Read) {
      desc.type = NRFX_TWIM_XFER_TXRX;
    } else {
      desc.type = NRFX_TWIM_XFER_TXTX;
    }
    desc.primary_length = 1;
    desc.p_primary_buf = &bus->state->transfer.register_address;
    
    desc.secondary_length = bus->state->transfer.size;
    desc.p_secondary_buf = bus->state->transfer.data;
  } else {
    if (bus->state->transfer.direction == I2CTransferDirection_Read) {
      desc.type = NRFX_TWIM_XFER_RX;
    } else {
      desc.type = NRFX_TWIM_XFER_TX;
    }
    desc.primary_length = bus->state->transfer.size;
    desc.p_primary_buf = bus->state->transfer.data;
    desc.secondary_length = 0;
  }
  
  nrfx_err_t rv = nrfx_twim_xfer(&bus->hal->twim, &desc, 0);
  PBL_ASSERTN(rv == NRFX_SUCCESS);
}
