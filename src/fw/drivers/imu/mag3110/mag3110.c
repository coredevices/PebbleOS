/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mag3110.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/mag.h"
#include "drivers/periph_config.h"
#include "kernel/events.h"
#include "system/logging.h"
#include "os/mutex.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

#include <inttypes.h>
#include <stdint.h>

static PebbleMutex *s_mag_mutex;

static bool s_initialized = false;

static int s_use_refcount = 0;

// MAG3110 Register Address Map
#define DR_STATUS_REG    0x00
#define OUT_X_MSB_REG    0x01 // a 6-byte read here will return X, Y, Z data
#define WHO_AM_I_REG     0x07
#define SYSMOD_REG       0x08
#define CTRL_REG1        0x10
#define CTRL_REG2        0x11

static bool mag3110_read(uint8_t reg_addr, uint8_t data_len, uint8_t *data) {
  return i2c_read_register_block(I2C_MAG3110, reg_addr, data_len, data);
}

static bool mag3110_write(uint8_t reg_addr, uint8_t data) {
  return i2c_write_register_block(I2C_MAG3110, reg_addr, 1, &data);
}

static void mag3110_interrupt_handler(bool *should_context_switch) {
  if (s_use_refcount == 0) {
    // Spurious interrupt firing after we've already turned off the mag. Just ignore.
    return;
  }

  // TODO: May want to use a timer, lowers worst case latency
  PebbleEvent e = {
    .type = PEBBLE_ECOMPASS_SERVICE_EVENT,
  };

  *should_context_switch = event_put_isr(&e);
}

//! Move the mag into standby mode, which is a low power mode where we're not actively sampling
//! the sensor or firing interrupts.
static bool prv_enter_standby_mode(void) {
  // Ask to enter standby mode
  if (!mag3110_write(CTRL_REG1, 0x00)) {
    return false;
  }

  // Wait for the sysmod register to read that we're now in standby mode. This can take up to
  // 1/ODR to respond. Since we only support speeds as slow as 5Hz, that means we may be waiting
  // for up to 200ms for this part to become ready.
  const int NUM_ATTEMPTS = 300; // 200ms + some padding for safety
  for (int i = 0; i < NUM_ATTEMPTS; ++i) {
    uint8_t sysmod = 0;
    if (!mag3110_read(SYSMOD_REG, 1, &sysmod)) {
      return false;
    }

    if (sysmod == 0) {
      // We're done and we're now in standby!
      return true;
    }

    // Wait at least 1ms before asking again
    psleep(2);
  }

  return false;
}

// Ask the compass for a 8-bit value that's programmed into the IC at the
// factory. Useful as a sanity check to make sure everything came up properly.
bool mag3110_check_whoami(void) {
  static const uint8_t COMPASS_WHOAMI_BYTE = 0xc4;

  uint8_t whoami = 0;

  mag_use();
  mag3110_read(WHO_AM_I_REG, 1, &whoami);
  mag_release();

  PBL_LOG(LOG_LEVEL_DEBUG, "Read compass whoami byte 0x%x, expecting 0x%x",
      whoami, COMPASS_WHOAMI_BYTE);

  return (whoami == COMPASS_WHOAMI_BYTE);
}

void mag3110_init(void) {
  if (s_initialized) {
    return;
  }
  s_mag_mutex = mutex_create();

  s_initialized = true;

  if (!mag3110_check_whoami()) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to query Mag");
  }
  gpio_input_init(&BOARD_CONFIG_MAG.mag_int_gpio);

  exti_configure_pin(BOARD_CONFIG_MAG.mag_int, ExtiTrigger_Rising, mag3110_interrupt_handler);
}

void mag_use(void) {
  PBL_ASSERTN(s_initialized);

  mutex_lock(s_mag_mutex);

  if (s_use_refcount == 0) {
    i2c_use(I2C_MAG3110);
    exti_enable(BOARD_CONFIG_MAG.mag_int);
  }
  ++s_use_refcount;

  mutex_unlock(s_mag_mutex);
}

void mag_release(void) {
  PBL_ASSERTN(s_initialized && s_use_refcount != 0);

  mutex_lock(s_mag_mutex);

  --s_use_refcount;
  if (s_use_refcount == 0) {
    // We need to put the magnetometer into standby mode and read the data register to reset
    // the state so it's ready for next time.
    prv_enter_standby_mode();

    uint8_t raw_data[7];
    // DR_STATUS_REG is immediately before data registers
    mag3110_read(DR_STATUS_REG, sizeof(raw_data), raw_data);

    // Now we can actually remove power and disable the interrupt
    i2c_release(I2C_MAG3110);
    exti_disable(BOARD_CONFIG_MAG.mag_int);
  }

  mutex_unlock(s_mag_mutex);
}

// aligns magnetometer data with the coordinate system we have adopted
// for the watch
static int16_t align_coord_system(int axis, uint8_t *raw_data) {
  int offset = 2 * BOARD_CONFIG_MAG.mag_config.axes_offsets[axis];
  bool do_invert = BOARD_CONFIG_MAG.mag_config.axes_inverts[axis];
  int16_t mag_field_strength = ((raw_data[offset] << 8) | raw_data[offset + 1]);
  mag_field_strength *= (do_invert ? -1 : 1);
  return (mag_field_strength);
}

// callers responsibility to know if there is valid data to be read
MagReadStatus mag_read_data(MagData *data) {
  mutex_lock(s_mag_mutex);

  if (s_use_refcount == 0) {
    mutex_unlock(s_mag_mutex);
    return (MagReadMagOff);
  }

  MagReadStatus rv = MagReadSuccess;
  uint8_t raw_data[7];

  // DR_STATUS_REG is immediately before data registers
  if (!mag3110_read(DR_STATUS_REG, sizeof(raw_data), raw_data)) {
    rv = MagReadCommunicationFail;
    goto done;
  }

  // TODO: shouldn't happen at low sample rate, but handle case where some data
  // is overwritten
  if ((raw_data[0] & 0xf0) != 0) {
    PBL_LOG(LOG_LEVEL_INFO, "Some Mag Sample Data was overwritten, "
            "dr_status=0x%x", raw_data[0]);
    rv = MagReadClobbered; // we still need to read the data to clear the int
  }

  // map raw data to watch coord system
  data->x = align_coord_system(0, &raw_data[1]);
  data->y = align_coord_system(1, &raw_data[1]);
  data->z = align_coord_system(2, &raw_data[1]);

done:
  mutex_unlock(s_mag_mutex);
  return (rv);
}

bool mag_change_sample_rate(MagSampleRate rate) {
  mutex_lock(s_mag_mutex);

  if (s_use_refcount == 0) {
    mutex_unlock(s_mag_mutex);
    return (true);
  }

  bool success = false;

  // Enter standby state since we can only change sample rate in this mode.
  if (!prv_enter_standby_mode()) {
    goto done;
  }

  // See Table 25 in the data sheet for these values for the CTRL_REG1 register. We leave the
  // oversampling values at zero and just set the data rate bits.
  uint8_t new_sample_rate_value = 0;
  switch(rate) {
    case MagSampleRate20Hz:
      new_sample_rate_value = 0x1 << 6;
      break;
    case MagSampleRate5Hz:
      new_sample_rate_value = 0x2 << 6;
      break;
  }

  // Write the new sample rate as well as set the bottom bit of the ctrl register to put us into
  // active mode.
  if (!mag3110_write(CTRL_REG1, new_sample_rate_value | 0x01)) {
    goto done;
  }

  success = true;
done:
  mutex_unlock(s_mag_mutex);

  return (success);
}

void mag_start_sampling(void) {
  mag_use();

  // enable automatic magnetic sensor reset & RAW mode
  mag3110_write(CTRL_REG2, 0xA0);

  mag_change_sample_rate(MagSampleRate5Hz);
}
