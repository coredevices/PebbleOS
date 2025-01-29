#include "bmm350.h"

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

#include <inttypes.h>
#include <stdint.h>

static PebbleMutex *s_mag_mutex;

static bool s_initialized = false;

static int s_use_refcount = 0;

#define REG_CHIP_ID           0x00
#define   REG_CHIP_ID_DEFAULT   0x33
#define REG_ERR_REG           0x02
#define REG_PAD_CTL           0x03
#define REG_PMU_CMD_AGGR_SET  0x04
#define REG_PMU_CMD_AXIS_EN   0x05
#define REG_PMU_CMD           0x06
#define REG_PMU_CMD_STATUS_0  0x07
#define REG_PMU_CMD_STATUS_1  0x08
#define REG_I3C_ERR           0x09
#define REG_I2C_WDT_SET       0x0A
#define REG_TRANSDUCER_REV_ID 0x0D
#define REG_INT_CTRL          0x2E
#define REG_INT_CTRL_IBI      0x2F
#define REG_INT_STATUS        0x30
#define REG_MAG_X_XLSB        0x31
#define REG_MAG_X_LSB         0x32
#define REG_MAG_X_MSB         0x33
#define REG_MAG_Y_XLSB        0x34
#define REG_MAG_Y_LSB         0x35
#define REG_MAG_Y_MSB         0x36
#define REG_MAG_Z_XLSB        0x37
#define REG_MAG_Z_LSB         0x38
#define REG_MAG_Z_MSB         0x39
#define REG_TEMP_XLSB         0x3A
#define REG_TEMP_LSB          0x3B
#define REG_TEMP_MSB          0x3C
#define REG_SENSORTIME_XLSB   0x3D
#define REG_SENSORTIME_LSB    0x3E
#define REG_SENSORTIME_MSB    0x3F
#define REG_OTP_CMD_REG       0x50
#define REG_OTP_DATA_MSB_REG  0x52
#define REG_OTP_DATA_LSB_REG  0x53
#define REG_OTP_STATUS_REG    0x55
#define REG_TMR_SELFTEST_USER 0x60
#define REG_CTRL_USER         0x61
#define REG_CMD               0x7E
#define   REG_CMD_SOFT_RESET    0xB6

#define POR_DELAY_MS       3
#define SOFTRESET_DELAY_MS 24

#define READ_DUMMY_BYTES 2

static bool bmm350_read(uint8_t reg_addr, uint8_t data_len, uint8_t *data) {
  return i2c_read_register_block(I2C_BMM350, reg_addr, data_len, data);
}

static bool bmm350_write(uint8_t reg_addr, uint8_t data) {
  return i2c_write_register_block(I2C_BMM350, reg_addr, 1, &data);
}

static void bmm350_interrupt_handler(bool *should_context_switch) {
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
  if (!bmm350_write(REG_PMU_CMD, 0x00 /* SUS */)) {
    return false;
  }

  // Wait for the PMU to read that we're now in standby mode.
  const int NUM_ATTEMPTS = 300; // 200ms + some padding for safety
  for (int i = 0; i < NUM_ATTEMPTS; ++i) {
    uint8_t pmu_cmd_status = 0;
    if (!bmm350_read(REG_PMU_CMD_STATUS_0, 1, &pmu_cmd_status)) {
      return false;
    }

    if ((pmu_cmd_status & 0x9) == 0 /* pmu_cmd_busy | pwr_mode_is_normal */) {
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
bool bmm350_reset_check(void) {
  uint8_t rbuf[1 + READ_DUMMY_BYTES];

  mag_use();
  psleep(POR_DELAY_MS);
  if (!bmm350_write(REG_CMD, REG_CMD_SOFT_RESET)) {
    PBL_LOG(LOG_LEVEL_WARNING, "failed to write soft reset command");
    goto bailout;
  }
  psleep(SOFTRESET_DELAY_MS);
    
  if (!bmm350_read(REG_CHIP_ID, 1 + READ_DUMMY_BYTES, rbuf)) {
    PBL_LOG(LOG_LEVEL_WARNING, "failed to read from bmm350 over I2C");
    goto bailout;
  }
  
  /* XXX: download OTP */
  /* XXX: turn off OTP */
  /* XXX: magnetic_reset_and_wait */
  
  mag_release();

  PBL_LOG(LOG_LEVEL_DEBUG, "Read compass whoami byte 0x%x, expecting 0x%x",
      rbuf[READ_DUMMY_BYTES], REG_CHIP_ID_DEFAULT);

  return (rbuf[READ_DUMMY_BYTES] == REG_CHIP_ID_DEFAULT);
bailout:
  mag_release();
  return false;
}

void bmm350_init(void) {
  if (s_initialized) {
    return;
  }
  s_mag_mutex = mutex_create();

  s_initialized = true;

  gpio_input_init(&BOARD_CONFIG_MAG.mag_int_gpio);
  exti_configure_pin(BOARD_CONFIG_MAG.mag_int, ExtiTrigger_Falling, bmm350_interrupt_handler);

  if (!bmm350_reset_check()) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to query Mag");
  }
}

void mag_use(void) {
  PBL_ASSERTN(s_initialized);

  mutex_lock(s_mag_mutex);

  if (s_use_refcount == 0) {
    i2c_use(I2C_BMM350);
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

    // Now we can actually remove power and disable the interrupt
    i2c_release(I2C_BMM350);
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
  return MagReadMagOff;
#if 0
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
#endif
}

bool mag_change_sample_rate(MagSampleRate rate) {
#if 0
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
#else
  return false;
#endif
}

void mag_start_sampling(void) {
#if 0
  mag_use();

  // enable automatic magnetic sensor reset & RAW mode
  mag3110_write(CTRL_REG2, 0xA0);

  mag_change_sample_rate(MagSampleRate5Hz);
#endif
}
