/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/misc.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"
#include "system/passert.h"
#include "system/logging.h"
#include "util/delay.h"

#include <inttypes.h>

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_gpio.h"
#include "stm32f2xx_rcc.h"
#include "stm32f2xx_i2c.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"
#include "drivers/pmic.h"
#endif

#define portBASE_TYPE int
#define pdFALSE       0
#define portEND_SWITCHING_ISR(expr)   (void)(expr)

#define I2C_ERROR_TIMEOUT_MS  (1000)
#define I2C_TIMEOUT_ATTEMPTS_MAX (2 * 1000 * 1000)
#define I2C_NORMAL_MODE_CLOCK_SPEED_MAX   (100000)
#define I2C_NACK_COUNT_MAX    (1000) // MFI NACKs while busy. We delay ~1ms between retries so this is approximately a 1s timeout

#define I2C_READ_WRITE_BIT    (0x01)

typedef struct I2cTransfer {
  uint8_t device_address;
  bool    read_not_write;        //True for read, false for write
  uint8_t register_address;
  uint8_t size;
  uint8_t idx;
  uint8_t *data;
  enum TransferState {
    TRANSFER_STATE_WRITE_ADDRESS_TX,
    TRANSFER_STATE_WRITE_REG_ADDRESS,
    TRANSFER_STATE_REPEAT_START,
    TRANSFER_STATE_WRITE_ADDRESS_RX,
    TRANSFER_STATE_WAIT_FOR_DATA,
    TRANSFER_STATE_READ_DATA,
    TRANSFER_STATE_WRITE_DATA,
    TRANSFER_STATE_END_WRITE,

    TRANSFER_STATE_INVALID,
  } state;
  bool result;
  uint16_t nack_count;
}I2cTransfer;

typedef struct I2cBus{
  I2C_TypeDef         *i2c;
  uint8_t             user_count;
  I2cTransfer         transfer;
  volatile bool       busy;
}I2cBus;

static I2cBus i2c_buses[BOARD_I2C_BUS_COUNT];

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

static bool s_initialized = false;

/*----------------SEMAPHORE/LOCKING FUNCTIONS--------------------------*/

static void bus_lock(I2cBus *bus) {
}

static void bus_unlock(I2cBus *bus) {
}

static bool semaphore_take(I2cBus *bus) {
  return true;
}

static bool semaphore_wait(I2cBus *bus) {
  bus->busy = true;
  volatile uint32_t timeout_attempts = I2C_TIMEOUT_ATTEMPTS_MAX;
  while ((timeout_attempts-- > 0) && (bus->busy));
  bus->busy = false;
  return (timeout_attempts != 0);
}

static void semaphore_give(I2cBus *bus) {
}

/*-------------------BUS/PIN CONFIG FUNCTIONS--------------------------*/

//! Configure bus power supply control pin as output
//! Lock bus and peripheral config access before configuring pins
void i2c_bus_rail_ctl_config(OutputConfig pin_config) {
  gpio_use(pin_config.gpio);

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_StructInit(&GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = pin_config.gpio_pin;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(pin_config.gpio, &GPIO_InitStructure);

  gpio_release(pin_config.gpio);
}

//! Configure bus pins for use by I2C peripheral
//! Lock bus and peripheral config access before configuring pins
static void bus_pin_cfg_i2c(AfConfig pin_config) {
  gpio_use(pin_config.gpio);

  GPIO_InitTypeDef gpio_init_struct;
  gpio_init_struct.GPIO_Pin = pin_config.gpio_pin;
  gpio_init_struct.GPIO_Mode = GPIO_Mode_AF;
  gpio_init_struct.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_init_struct.GPIO_OType = GPIO_OType_OD;
  gpio_init_struct.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(pin_config.gpio, &gpio_init_struct);

  GPIO_PinAFConfig(pin_config.gpio, pin_config.gpio_pin_source, pin_config.gpio_af);

  gpio_release(pin_config.gpio);
}

//! Configure bus pin as input
//! Lock bus and peripheral config access before use
static void bus_pin_cfg_input(AfConfig pin_config) {
  gpio_use(pin_config.gpio);

  // Configure pin as high impedance input
  GPIO_InitTypeDef gpio_init_struct;
  gpio_init_struct.GPIO_Pin = pin_config.gpio_pin;
  gpio_init_struct.GPIO_Mode = GPIO_Mode_IN;
  gpio_init_struct.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init_struct.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(pin_config.gpio, &gpio_init_struct);

  gpio_release(pin_config.gpio);
}

//! Configure bus pin as output
//! Lock bus and peripheral config access before use
static void bus_pin_cfg_output(AfConfig pin_config, bool pin_state) {
  gpio_use(pin_config.gpio);

  // Configure pin as output
  GPIO_InitTypeDef gpio_init_struct;
  gpio_init_struct.GPIO_Pin = pin_config.gpio_pin;
  gpio_init_struct.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init_struct.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init_struct.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(pin_config.gpio, &gpio_init_struct);

  // Set bit high or low
  GPIO_WriteBit(pin_config.gpio, pin_config.gpio_pin, (pin_state) ? Bit_SET : Bit_RESET);

  gpio_release(pin_config.gpio);
}

//! Power down I2C bus power supply
//! Always lock bus and peripheral config access before use
static void bus_rail_power_down(uint8_t bus_idx) {
  if (BOARD_CONFIG.i2c_bus_configs[bus_idx].rail_ctl_fn == NULL) {
    return;
  }

  BOARD_CONFIG.i2c_bus_configs[bus_idx].rail_ctl_fn(false);

  // Drain through pull-ups
  bus_pin_cfg_output(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_scl, false);
  bus_pin_cfg_output(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_sda, false);
}

//! Power up I2C bus power supply
//! Always lock bus and peripheral config access before use
static void bus_rail_power_up(uint8_t bus_idx) {
  if (BOARD_CONFIG.i2c_bus_configs[bus_idx].rail_ctl_fn == NULL) {
    return;
  }

  // check that at least enough time has elapsed since the last turn-off
  // TODO: is this necessary in bootloader?
  static const uint32_t MIN_STOP_TIME_MS = 10;
  delay_ms(MIN_STOP_TIME_MS);

  bus_pin_cfg_input(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_scl);
  bus_pin_cfg_input(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_sda);

  BOARD_CONFIG.i2c_bus_configs[bus_idx].rail_ctl_fn(true);
}

//! Initialize the I2C peripheral
//! Lock the bus and peripheral config access before initialization
static void bus_init(uint8_t bus_idx) {
  // Initialize peripheral
  I2C_InitTypeDef i2c_init_struct;
  I2C_StructInit(&i2c_init_struct);

  if (BOARD_CONFIG.i2c_bus_configs[bus_idx].clock_speed > I2C_NORMAL_MODE_CLOCK_SPEED_MAX) {  //Fast mode
    i2c_init_struct.I2C_DutyCycle = BOARD_CONFIG.i2c_bus_configs[bus_idx].duty_cycle;
  }
  i2c_init_struct.I2C_ClockSpeed = BOARD_CONFIG.i2c_bus_configs[bus_idx].clock_speed;
  i2c_init_struct.I2C_Ack = I2C_Ack_Enable;

  I2C_Init(i2c_buses[bus_idx].i2c, &i2c_init_struct);
  I2C_Cmd(i2c_buses[bus_idx].i2c, ENABLE);
}

//! Configure the bus pins, enable the peripheral clock and initialize the I2C peripheral.
//! Always lock the bus and peripheral config access before enabling it
static void bus_enable(uint8_t bus_idx) {
  // Don't power up rail if the bus is already in use (enable can be called to reset bus)
  if (i2c_buses[bus_idx].user_count ==  0) {
    bus_rail_power_up(bus_idx);
  }

  bus_pin_cfg_i2c(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_scl);
  bus_pin_cfg_i2c(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_sda);

  // Enable peripheral clock
  periph_config_acquire_lock();
  periph_config_enable(RCC_APB1PeriphClockCmd, BOARD_CONFIG.i2c_bus_configs[bus_idx].clock_ctrl);
  periph_config_release_lock();

  bus_init(bus_idx);
}

//! De-initialize and gate the clock to the peripheral
//! Power down rail if the bus supports that and no devices are using it
//! Always lock the bus and peripheral config access before disabling it
static void bus_disable(uint8_t bus_idx) {
  I2C_DeInit(i2c_buses[bus_idx].i2c);

  periph_config_acquire_lock();
  periph_config_disable(RCC_APB1PeriphClockCmd, BOARD_CONFIG.i2c_bus_configs[bus_idx].clock_ctrl);
  periph_config_release_lock();

  // Do not de-power rail if there are still devices using bus (just reset peripheral and pin configuration during a bus reset)
  if (i2c_buses[bus_idx].user_count == 0) {
    bus_rail_power_down(bus_idx);
  }
  else {
    bus_pin_cfg_input(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_scl);
    bus_pin_cfg_input(BOARD_CONFIG.i2c_bus_configs[bus_idx].i2c_sda);
  }
}

//! Perform a soft reset of the bus
//! Always lock the bus before reset
static void bus_reset(uint8_t bus_idx) {
  bus_disable(bus_idx);
  bus_enable(bus_idx);
}

/*---------------INIT/USE/RELEASE/RESET FUNCTIONS----------------------*/

void i2c_init(void) {
  for (uint32_t i = 0; i < ARRAY_LENGTH(i2c_buses); i++) {
    i2c_buses[i].i2c = BOARD_CONFIG.i2c_bus_configs[i].i2c;
    i2c_buses[i].user_count = 0;
    i2c_buses[i].busy = false;
    i2c_buses[i].transfer.idx = 0;
    i2c_buses[i].transfer.size = 0;
    i2c_buses[i].transfer.data = NULL;
    i2c_buses[i].transfer.state = TRANSFER_STATE_INVALID;

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = BOARD_CONFIG.i2c_bus_configs[i].ev_irq_channel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x0c;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = BOARD_CONFIG.i2c_bus_configs[i].er_irq_channel;
    NVIC_Init(&NVIC_InitStructure);

    I2C_DeInit(i2c_buses[i].i2c);
  }

  s_initialized = true;

  for (uint32_t i = 0; i < ARRAY_LENGTH(i2c_buses); i++) {
    if (BOARD_CONFIG.i2c_bus_configs[i].rail_cfg_fn) {
      BOARD_CONFIG.i2c_bus_configs[i].rail_cfg_fn();
    }
    if (BOARD_CONFIG.i2c_bus_configs[i].rail_ctl_fn) {
      bus_rail_power_down(i);
    }
  }
}

void i2c_use(I2cDevice device_id) {
  PBL_ASSERTN(s_initialized);
  PBL_ASSERT(device_id < BOARD_CONFIG.i2c_device_count, "I2C device ID out of bounds %d (max: %d)",
      device_id, BOARD_CONFIG.i2c_device_count);

  uint8_t bus_idx = BOARD_CONFIG.i2c_device_map[device_id];
  I2cBus *bus = &i2c_buses[bus_idx];

  bus_lock(bus);

  if (bus->user_count == 0) {
    bus_enable(bus_idx);
  }
  bus->user_count++;

  bus_unlock(bus);
}

void i2c_release(I2cDevice device_id) {
  PBL_ASSERTN(s_initialized);
  PBL_ASSERTN(device_id < BOARD_CONFIG.i2c_device_count);

  uint8_t bus_idx = BOARD_CONFIG.i2c_device_map[device_id];
  I2cBus *bus = &i2c_buses[bus_idx];

  bus_lock(bus);

  if (bus->user_count == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Attempted release of disabled bus %d by device %d", bus_idx, device_id);
    bus_unlock(bus);
    return;
  }

  bus->user_count--;
  if (bus->user_count == 0) {
    bus_disable(bus_idx);
  }

  bus_unlock(bus);
}

void i2c_reset(I2cDevice device_id) {
  PBL_ASSERTN(s_initialized);
  PBL_ASSERTN(device_id < BOARD_CONFIG.i2c_device_count);

  uint8_t bus_idx = BOARD_CONFIG.i2c_device_map[device_id];
  I2cBus *bus = &i2c_buses[bus_idx];

  // Take control of bus; only one task may use bus at a time
  bus_lock(bus);

  if (bus->user_count == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Attempted reset of disabled bus %d by device %d", bus_idx, device_id);
    bus_unlock(bus);
    return;
  }

  PBL_LOG(LOG_LEVEL_WARNING, "Resetting I2C bus %" PRId8, bus_idx);

  // decrement user count for reset so that if this user is the only user, the
  // bus will be powered down during the reset
  bus->user_count--;

  // Reset and reconfigure bus and pins
  bus_reset(bus_idx);

  //Restore user count
  bus->user_count++;

  bus_unlock(bus);
}

/*--------------------DATA TRANSFER FUNCTIONS--------------------------*/

//! Wait a short amount of time for busy bit to clear
static bool wait_for_busy_clear(uint8_t bus_idx) {
  unsigned int attempts = I2C_TIMEOUT_ATTEMPTS_MAX;
  while((i2c_buses[bus_idx].i2c->SR2 & I2C_SR2_BUSY) != 0) {
    --attempts;
    if (!attempts) {
      return false;
    }
  }

  return true;
}

//! Abort the transfer
//! Should only be called when the bus is locked
static void abort_transfer(I2cBus *bus) {
  // Disable all interrupts on the bus
  bus->i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
  // Generate a stop condition
  bus->i2c->CR1 |= I2C_CR1_STOP;
  bus->transfer.state = TRANSFER_STATE_INVALID;
}

//! Set up and start a transfer to a device, wait for it to finish and clean up after the transfer has completed
static bool do_transfer(I2cDevice device_id, bool read_not_write, uint8_t device_address, uint8_t register_address, uint8_t size, uint8_t *data) {
  PBL_ASSERTN(s_initialized);
  PBL_ASSERTN(device_id < BOARD_CONFIG.i2c_device_count);

  uint8_t bus_idx = BOARD_CONFIG.i2c_device_map[device_id];
  I2cBus *bus = &i2c_buses[bus_idx];

  // Take control of bus; only one task may use bus at a time
  bus_lock(bus);

  if (bus->user_count == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Attempted access to disabled bus %d by device %d", bus_idx, device_id);
    bus_unlock(bus);
    return false;
  }

  // If bus is busy (it shouldn't be as this function waits for the bus to report a non-idle state
  // before exiting) reset the bus and wait for it to become not-busy
  // Exit if bus remains busy. User module should reset the I2C module at this point
  if((bus->i2c->SR2 & I2C_SR2_BUSY) != 0) {
    bus_reset(bus_idx);

    if (!wait_for_busy_clear(bus_idx)) {
      // Bus did not recover after reset
      bus_unlock(bus);
      return false;
    }
  }

  // Take binary semaphore so that next take will block
  PBL_ASSERT(semaphore_take(bus), "Could not acquire semaphore token");

  // Set up transfer
  bus->transfer.device_address = device_address;
  bus->transfer.register_address = register_address;
  bus->transfer.read_not_write = read_not_write;
  bus->transfer.size = size;
  bus->transfer.idx = 0;
  bus->transfer.state = TRANSFER_STATE_WRITE_ADDRESS_TX;
  bus->transfer.data = data;
  bus->transfer.nack_count = 0;

  // Ack received bytes
  I2C_AcknowledgeConfig(bus->i2c, ENABLE);

  bool result = false;

  do {
    // Generate start event
    bus->i2c->CR1 |= I2C_CR1_START;
    //Enable event and error interrupts
    bus->i2c->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;

    // Wait on semaphore until it is released by interrupt or a timeout occurs
    if (semaphore_wait(bus)) {

      if (bus->transfer.state == TRANSFER_STATE_INVALID) {
        // Transfer is complete
        result = bus->transfer.result;
        if (!result) {
          PBL_LOG(LOG_LEVEL_ERROR, "I2C Error on bus %" PRId8, bus_idx);
        }

      } else if (bus->transfer.nack_count < I2C_NACK_COUNT_MAX) {
        // NACK received after start condition sent: the MFI chip NACKs start conditions whilst it is busy
        // Retry start condition after a short delay.
        // A NACK count is incremented for each NACK received, so that legitimate NACK
        // errors cause the transfer to be aborted (after the NACK count max has been reached).

        bus->transfer.nack_count++;

        delay_ms(1);

      } else {
        // Too many NACKs received, abort transfer
        abort_transfer(bus);
        break;
        PBL_LOG(LOG_LEVEL_ERROR, "I2C Error: too many NACKs received on bus %" PRId8, bus_idx);
      }

    } else {
      // Timeout, abort transfer
      abort_transfer(bus);
      break;
      PBL_LOG(LOG_LEVEL_ERROR, "Transfer timed out on bus %" PRId8, bus_idx);
    }
  } while (bus->transfer.state != TRANSFER_STATE_INVALID);

  // Return semaphore token so another transfer can be started
  semaphore_give(bus);

  // Wait for bus to to clear the busy flag before a new transfer starts
  // Theoretically a transfer could complete successfully, but the busy flag never clears,
  // which would cause the next transfer to fail
  if (!wait_for_busy_clear(bus_idx)) {
    // Reset I2C bus if busy flag does not clear
    bus_reset(bus_idx);
  }

  bus_unlock(bus);

  return result;
}

bool i2c_read_register(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address, uint8_t *result) {
  return i2c_read_register_block(device_id, i2c_device_address, register_address, 1, result);
}

bool i2c_read_register_block(I2cDevice device_id, uint8_t i2c_device_address, uint8_t
                             register_address_start, uint8_t read_size, uint8_t* result_buffer) {
#if defined(TARGET_QEMU)
  PBL_LOG(LOG_LEVEL_DEBUG, "i2c reads on QEMU not supported");
  return false;
#endif

  // Do transfer locks the bus
  bool result = do_transfer(device_id, true, i2c_device_address, register_address_start, read_size, result_buffer);

  if (!result) {
    PBL_LOG(LOG_LEVEL_ERROR, "Read failed on bus %" PRId8, BOARD_CONFIG.i2c_device_map[device_id]);
  }

  return result;
}

bool i2c_write_register(I2cDevice device_id, uint8_t i2c_device_address, uint8_t register_address,
                         uint8_t value) {
  return i2c_write_register_block(device_id, i2c_device_address, register_address, 1, &value);
}

bool i2c_write_register_block(I2cDevice device_id, uint8_t i2c_device_address, uint8_t
                              register_address_start, uint8_t write_size, const uint8_t* buffer) {
#if defined(TARGET_QEMU)
    PBL_LOG(LOG_LEVEL_DEBUG, "i2c writes on QEMU not supported");
    return false;
#endif

  // Do transfer locks the bus
  bool result = do_transfer(device_id, false, i2c_device_address, register_address_start, write_size, (uint8_t*)buffer);

  if (!result) {
    PBL_LOG(LOG_LEVEL_ERROR, "Write failed on bus %" PRId8, BOARD_CONFIG.i2c_device_map[device_id]);
  }

  return result;
}

/*------------------------INTERRUPT FUNCTIONS--------------------------*/

//! End a transfer and disable further interrupts
//! Only call from interrupt functions
static portBASE_TYPE end_transfer_irq(I2cBus *bus, bool result) {
  bus->i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
  bus->i2c->CR1 |= I2C_CR1_STOP;
  bus->transfer.result = result;
  bus->transfer.state = TRANSFER_STATE_INVALID;

  bus->busy = false;
  return pdFALSE;
}

//! Pause a transfer, disabling interrupts during the pause
//! Only call from interrupt functions
static portBASE_TYPE pause_transfer_irq(I2cBus *bus) {
  bus->i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN);
  bus->busy = false;
  return pdFALSE;
}

//! Handle an IRQ event on the specified \a bus
static portBASE_TYPE irq_event_handler(I2cBus *bus) {
  if (bus->transfer.state == TRANSFER_STATE_INVALID) {

    // Disable interrupts if spurious interrupt received
    bus->i2c->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN);
    return pdFALSE;
  }

  // Check that the expected event occurred
  if (I2C_CheckEvent(bus->i2c, s_guard_events[bus->transfer.state]) == ERROR) {
    // Ignore interrupt - A spurious byte transmitted event as well as an interrupt with no
    // discernible event associated with it occur after repeat start events are generated
    return pdFALSE;
  }
  portBASE_TYPE should_context_switch = pdFALSE;

  switch (bus->transfer.state) {
    case TRANSFER_STATE_WRITE_ADDRESS_TX:
      // Write the i2c device address to the bus to select it in write mode.
      bus->i2c->DR = bus->transfer.device_address & ~I2C_READ_WRITE_BIT;
      bus->transfer.state = TRANSFER_STATE_WRITE_REG_ADDRESS;
      break;

    case TRANSFER_STATE_WRITE_REG_ADDRESS:
      // Write the register address
      bus->i2c->DR = bus->transfer.register_address;

      if (bus->transfer.read_not_write) {
        bus->transfer.state = TRANSFER_STATE_REPEAT_START;
      } else {
        // Enable TXE interrupt for writing
        bus->i2c->CR2 |= I2C_CR2_ITBUFEN;
        bus->transfer.state = TRANSFER_STATE_WRITE_DATA;
      }
      break;

    case TRANSFER_STATE_REPEAT_START:
      // Generate a repeat start
      bus->i2c->CR1 |= I2C_CR1_START;
      bus->transfer.state = TRANSFER_STATE_WRITE_ADDRESS_RX;
      break;

    case TRANSFER_STATE_WRITE_ADDRESS_RX:
      // Write the I2C device address again, but this time in read mode.
      bus->i2c->DR = bus->transfer.device_address | I2C_READ_WRITE_BIT;
      if (bus->transfer.size == 1) {
        // Last byte, we want to NACK this one to tell the slave to stop sending us bytes.
        bus->i2c->CR1 &= ~I2C_CR1_ACK;
      }
      bus->transfer.state = TRANSFER_STATE_WAIT_FOR_DATA;
      break;

    case TRANSFER_STATE_WAIT_FOR_DATA:
      //This state just ensures that the transition to receive mode event happened

      // Enable RXNE interrupt for writing
      bus->i2c->CR2 |= I2C_CR2_ITBUFEN;
      bus->transfer.state = TRANSFER_STATE_READ_DATA;
      break;

    case TRANSFER_STATE_READ_DATA:
      bus->transfer.data[bus->transfer.idx] = bus->i2c->DR;
      bus->transfer.idx++;

      if (bus->transfer.idx + 1 == bus->transfer.size) {
        // Last byte, we want to NACK this one to tell the slave to stop sending us bytes.
        bus->i2c->CR1 &= ~I2C_CR1_ACK;
      }
      else if (bus->transfer.idx == bus->transfer.size) {
        // End transfer after all bytes have been received
        bus->i2c->CR2 &= ~I2C_CR2_ITBUFEN;
        should_context_switch = end_transfer_irq(bus, true);
        break;
      }

      break;

    case TRANSFER_STATE_WRITE_DATA:
      bus->i2c->DR = bus->transfer.data[bus->transfer.idx];
      bus->transfer.idx++;
      if (bus->transfer.idx == bus->transfer.size) {
        bus->i2c->CR2 &= ~I2C_CR2_ITBUFEN;
        bus->transfer.state = TRANSFER_STATE_END_WRITE;
        break;
      }
      break;

    case TRANSFER_STATE_END_WRITE:
      // End transfer after all bytes have been sent
      should_context_switch = end_transfer_irq(bus, true);
      break;

    default:
      // Abort transfer from invalid state - should never reach here (state machine logic broken)
      should_context_switch = end_transfer_irq(bus, false);
      break;
  }

  return should_context_switch;
}

//! Handle error interrupt on the specified \a bus
static portBASE_TYPE irq_error_handler(I2cBus *bus) {
  if (bus->transfer.state == TRANSFER_STATE_INVALID) {

    // Disable interrupts if spurious interrupt received
    bus->i2c->CR2 &= ~I2C_CR2_ITERREN;
    return pdFALSE;
  }

  // Data overrun and bus errors can only really be handled by terminating the transfer and
  // trying to recover bus to an idle state. Each error will be logged. In each case a stop
  // condition will be sent and then we will wait on the busy flag to clear (if it doesn't,
  // a soft reset of the bus will be performed (handled in wait i2c_do_transfer).

  if ((bus->i2c->SR1 & I2C_SR1_OVR) != 0) {
    bus->i2c->SR1 &= ~I2C_SR1_OVR;

    // Data overrun
    PBL_LOG(LOG_LEVEL_ERROR, "Data overrun during I2C transaction; Bus: 0x%p", bus->i2c);
  }
  if ((bus->i2c->SR1 & I2C_SR1_BERR) != 0) {
    bus->i2c->SR1 &= ~I2C_SR1_BERR;

    // Bus error: invalid start or stop condition detected
    PBL_LOG(LOG_LEVEL_ERROR, "Bus error detected during I2C transaction; Bus: 0x%p", bus->i2c);
  }
  if ((bus->i2c->SR1 & I2C_SR1_AF) != 0) {
    bus->i2c->SR1 &= ~I2C_SR1_AF;

    // NACK received.
    //
    // The MFI chip will cause NACK errors during read operations after writing a start bit (first start
    // or repeat start indicating that it is busy. The transfer must be paused and the start condition sent
    // again after a delay and the state machine set back a step.
    //
    // If the NACK is received after any other action log an error and abort the transfer


    if (bus->transfer.state == TRANSFER_STATE_WAIT_FOR_DATA) {
      bus->transfer.state = TRANSFER_STATE_WRITE_ADDRESS_RX;
      return pause_transfer_irq(bus);
    }
    else if (bus->transfer.state == TRANSFER_STATE_WRITE_REG_ADDRESS){
      bus->transfer.state = TRANSFER_STATE_WRITE_ADDRESS_TX;
      return pause_transfer_irq(bus);
    }
    else {
      PBL_LOG(LOG_LEVEL_ERROR, "NACK received during I2C transfer; Bus: 0x%p", bus->i2c);
    }
  }

  return end_transfer_irq(bus, false);

}

void I2C1_EV_IRQHandler(void) {
  portEND_SWITCHING_ISR(irq_event_handler(&i2c_buses[0]));
}

void I2C1_ER_IRQHandler(void) {
  portEND_SWITCHING_ISR(irq_error_handler(&i2c_buses[0]));
}

void I2C2_EV_IRQHandler(void) {
  portEND_SWITCHING_ISR(irq_event_handler(&i2c_buses[1]));
}

void I2C2_ER_IRQHandler(void) {
  portEND_SWITCHING_ISR(irq_error_handler(&i2c_buses[1]));
}

/*------------------------COMMAND FUNCTIONS--------------------------*/

void command_power_2v5(char *arg) {
  // Intentionally ignore the s_running_count and make it so!
  // This is intended for low level electrical test only
  if(!strcmp("on", arg)) {
    bus_rail_power_up(1);
  } else {
    bus_rail_power_down(1);
  }
}
