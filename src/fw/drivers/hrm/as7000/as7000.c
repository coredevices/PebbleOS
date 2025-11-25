/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define FILE_LOG_COLOR LOG_COLOR_GREEN
#include "as7000.h"

#include "board/board.h"
#include "drivers/backlight.h"
#include "drivers/exti.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "kernel/events.h"
#include "kernel/util/sleep.h"
#include "kernel/util/interval_timer.h"
#include "mfg/mfg_info.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/common/analytics/analytics.h"
#include "services/common/system_task.h"
#include "services/common/hrm/hrm_manager.h"
#include "system/logging.h"
#include "system/passert.h"
#include "system/profiler.h"
#include "util/attributes.h"
#include "util/ihex.h"
#include "util/math.h"
#include "util/net.h"

#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>

// Enable this to get some very verbose logs about collecting PPG data from the HRM
// Bump this up to 2 to get very verbose logs
#define PPG_DEBUG 0

#if PPG_DEBUG
#define PPG_DBG(...) \
  do { \
    PBL_LOG(LOG_LEVEL_DEBUG, __VA_ARGS__); \
  } while (0);
#else
#define PPG_DBG(...)
#endif

#if PPG_DEBUG == 2
#define PPG_DBG_VERBOSE(...) \
  do { \
    PBL_LOG_VERBOSE(LOG_LEVEL_DEBUG, __VA_ARGS__); \
  } while (0);
#else
#define PPG_DBG_VERBOSE(...)
#endif



// The datasheet recommends waiting for 250ms for the chip to boot
#define NORMAL_BOOT_DELAY_MS (250)
// We need to wait an extra second for the loader to time-out
#define LOADER_REBOOT_DELAY_MS (NORMAL_BOOT_DELAY_MS + 1000)
// Usually takes a couple ms after writing a record, but spikes of ~20ms have been observed. Let's
// be conservative.
#define LOADER_READY_MAX_DELAY_MS (50)
// Give the sensor this much time to tear down the current app and go back to the idle mode
#define SHUT_DOWN_DELAY_MS (1000)
// Number of handshakes before samples are expected
#define WARMUP_HANDSHAKES (2)

#define EXPECTED_PROTOCOL_VERSION_MAJOR (2)

// White Threshold is 5000
// Black Threshold is 3500
// Value stored in the register is in units of 64 ADC counts
// e.g. 78 * 64 = 4992 ADC-counts
// Refer to AS7000 SW Communication Protocol section 6.7
#define PRES_DETECT_THRSH_WHITE 78 // (5000 / 64)
#define PRES_DETECT_THRSH_BLACK 54 // (3500 / 64)

// register addresses
#define ADDR_LOADER_STATUS (0x00)
#define ADDR_INFO_START (0x00)
#define ADDR_APP_IDS (0x04)

#define ADDR_ACCEL_SAMPLE_FREQ_MSB (0x08)
#define ADDR_ACCEL_SAMPLE_FREQ_LSB (0x09)

// Register that allows us to compensate for clock skew between us (the host) and the sensor. The
// sensor doesn't track time accurately, and gives us a heart rate value that's in the sensors
// time domain, which will need to be translated into "real time" according to our time domain.
// If we use these registers to tell the sensor how frequently it's handshaking with us in our
// time domain, this will let the sensor do this compensation for us.
// The value programmed in here is in units of 0.1ms (value of 10000 = 1 second).
#define ADDR_HOST_ONE_SECOND_TIME_MSB (0x0a)
#define ADDR_HOST_ONE_SECOND_TIME_LSB (0x0b)

#define ADDR_NUM_ACCEL_SAMPLES (0x0e)
#define ADDR_NUM_PPG_SAMPLES (0x0f)

#define ADDR_ACCEL_SAMPLE_IDX (0x14)
#define ADDR_ACCEL_X_MSB (0x15)
#define ADDR_ACCEL_Y_MSB (0x17)
#define ADDR_ACCEL_Z_MSB (0x19)

#define ADDR_PPG_IDX (0x1b)
#define ADDR_PPG_MSB (0x1c)
#define ADDR_PPG_LSB (0x1d)
#define ADDR_TIA_MSB (0x1e)
#define ADDR_TIA_LSB (0x1f)

#define ADDR_PRES_DETECT_THRSH (0x26)

#define ADDR_LED_CURRENT_MSB (0x34)
#define ADDR_LED_CURRENT_LSB (0x35)
#define ADDR_HRM_STATUS (0x36)
#define ADDR_HRM_BPM    (0x37)
#define ADDR_HRM_SQI    (0x38)

#define ADDR_SYNC (0x39)

// The AS7000 wants Accel Frequency given in 0.001Hz increments, this can be used to scale
#define AS7000_ACCEL_FREQUENCY_SCALE (1000)

//! Thresholds for quality conversion. These are upper bounds on readings.
enum AS7000SQIThreshold {
  AS7000SQIThreshold_Excellent = 2,
  AS7000SQIThreshold_Good = 5,
  AS7000SQIThreshold_Acceptable = 8,
  AS7000SQIThreshold_Poor = 10,
  AS7000SQIThreshold_Worst = 20,

  AS7000SQIThreshold_OffWrist = 254,

  AS7000SQIThresholdInvalid,
};

enum AS7000Status {
  AS7000Status_OK = 0,
  AS7000Status_IllegalParameter = 1,
  AS7000Status_LostData = 2,
  AS7000Status_NoAccel = 4,
};

typedef enum AS7000AppId {
  AS7000AppId_Idle = 0x00,
  AS7000AppId_Loader = 0x01,
  AS7000AppId_HRM = 0x02,
  AS7000AppId_PRV = 0x04,
  AS7000AppId_GSR = 0x08,
  AS7000AppId_NTC = 0x10,
} AS7000AppId;

typedef enum AS7000LoaderStatus {
  AS7000LoaderStatus_Ready = 0x00,
  AS7000LoaderStatus_Busy1 = 0x3A,
  AS7000LoaderStatus_Busy2 = 0xFF,
  // all other values indicate an error
} AS7000LoaderStatus;

typedef struct PACKED AS7000FWUpdateHeader {
  uint8_t sw_version_major;
  uint8_t sw_version_minor;
} AS7000FWUpdateHeader;

typedef struct PACKED AS7000FWSegmentHeader {
  uint16_t address;
  uint16_t len_minus_1;
} AS7000FWSegmentHeader;

//! The maximum number of data bytes to include in a reconstituted
//! Intel HEX Data record when updating the HRM firmware.
//! This is the size of the binary data encoded in the record, __NOT__
//! the size of the HEX record encoding the data. The HEX record itself
//! will be IHEX_RECORD_LENGTH(MAX_HEX_DATA_BYTES)
//! (MAX_HEX_DATA_BYTES*2 + 11) bytes in size.
#define MAX_HEX_DATA_BYTES (96)

// The AS7000 loader cannot accept HEX records longer than 256 bytes.
_Static_assert(IHEX_RECORD_LENGTH(MAX_HEX_DATA_BYTES) <= 256,
               "The value of MAX_HEX_DATA_BYTES will result in HEX records "
               "which are longer than the AS7000 loader can handle.");


// The sw_version_major field is actually a bitfield encoding both the
// major and minor components of the SDK version number. Define macros
// to extract the components for logging purposes.
#define HRM_SW_VERSION_PART_MAJOR(v) (v >> 6)
#define HRM_SW_VERSION_PART_MINOR(v) (v & 0x3f)

// If this many watchdog interrupts occur before we receive an interrupt from the sensor,
// we assume the sensor requires a reset
#define AS7000_MAX_WATCHDOG_INTERRUPTS 5

// We use this regular timer as a watchdog for the sensor. We have seen cases where the sensor
// becomes unresponsive (PBL-40008). This timer watches to see if we have stopped receiving
// sensor interrupts and will trigger logic to reset the sensor if necessary.
static RegularTimerInfo s_as7000_watchdog_timer;

// Incremented by s_as7000_watchdog_timer. Reset to 0 by our interrupt handler.
static uint8_t s_missing_interrupt_count;

//! Interval timer to track how frequently the as7000 is handshaking with us
static IntervalTimer s_handshake_interval_timer;

static void prv_enable_timer_cb(void *context);
static void prv_disable_watchdog(HRMDevice *dev);

static bool prv_write_register(HRMDevice *dev, uint8_t register_address, uint8_t value) {
  i2c_use(dev->i2c_slave);
  bool rv = i2c_write_register(dev->i2c_slave, register_address, value);
  i2c_release(dev->i2c_slave);
  return rv;
}

static bool prv_write_register_block(HRMDevice *dev, uint8_t register_address,
                                     const void *buffer, uint32_t length) {
  i2c_use(dev->i2c_slave);
  bool rv = i2c_write_register_block(dev->i2c_slave, register_address, length, buffer);
  i2c_release(dev->i2c_slave);
  return rv;
}

static bool prv_read_register(HRMDevice *dev, uint8_t register_address, uint8_t *value) {
  i2c_use(dev->i2c_slave);
  bool rv = i2c_read_register(dev->i2c_slave, register_address, value);
  i2c_release(dev->i2c_slave);
  return rv;
}

static bool prv_read_register_block(HRMDevice *dev, uint8_t register_address, void *buffer,
                                    uint32_t length) {
  i2c_use(dev->i2c_slave);
  bool rv = i2c_read_register_block(dev->i2c_slave, register_address, length, buffer);
  i2c_release(dev->i2c_slave);
  return rv;
}

static bool prv_set_host_one_second_time_register(HRMDevice *dev, uint32_t average_ms) {
  PPG_DBG("host one second time: %"PRIu32" ms", average_ms);

  // Register takes a reading in 0.1ms increments
  uint16_t value = average_ms * 10;

  const uint8_t msb = (value >> 8) & 0xff;
  const uint8_t lsb = value & 0xff;
  return prv_write_register(dev, ADDR_HOST_ONE_SECOND_TIME_MSB, msb)
      && prv_write_register(dev, ADDR_HOST_ONE_SECOND_TIME_LSB, lsb);
}

static void prv_read_ppg_data(HRMDevice *dev, HRMPPGData *data_out) {
  uint8_t num_ppg_samples;
  prv_read_register(HRM, ADDR_NUM_PPG_SAMPLES, &num_ppg_samples);
  num_ppg_samples = MIN(num_ppg_samples, MAX_PPG_SAMPLES);

  for (int i = 0; i < num_ppg_samples; ++i) {
    struct PACKED {
      uint8_t idx;
      uint16_t ppg;
      uint16_t tia;
    } ppg_reading;

    // Reading PPG data from the chip is a little weird. We need to read the PPG block of registers
    // which maps to the ppg_reading struct above. We then need to verify that the index that we
    // read matches the one that we expect. If we attempt to read the registers too quickly back to
    // back that means that the AS7000 failed to update the value in time and we just need to try
    // again. Limit this to a fixed number of attempts to make sure we don't infinite loop.
    const int NUM_ATTEMPTS = 3;
    bool success = false;
    for (int j = 0; j < NUM_ATTEMPTS; ++j) {
      prv_read_register_block(HRM, ADDR_PPG_IDX, &ppg_reading, sizeof(ppg_reading));
      if (ppg_reading.idx == i + 1) {
        data_out->indexes[i] = ppg_reading.idx;
        data_out->ppg[i] = ntohs(ppg_reading.ppg);
        data_out->tia[i] = ntohs(ppg_reading.tia);

        success = true;
        break;
      }

      PPG_DBG_VERBOSE("FAIL: got %"PRIu16" expected %u tia %"PRIu16,
                      ppg_reading.idx, i + 1, ntohs(ppg_reading.tia));
      // Keep trying...
    }

    if (!success) {
      // We didn't find a sample, just give up on reading PPG for this handshake
      break;
    }

    data_out->num_samples++;
  }

  PPG_DBG("num_samples reg: %"PRIu8" read: %u",
          num_ppg_samples, data_out->num_samples);
}

static void prv_write_accel_sample(HRMDevice *dev, uint8_t sample_idx, AccelRawData *data) {
  struct PACKED {
    uint8_t sample_idx;
    net16 accel_x;
    net16 accel_y;
    net16 accel_z;
  } sample_data = {
    .sample_idx = sample_idx,
    .accel_x = hton16(data->x * 2), // Accel service supplies mGs, AS7000 expects lsb = 0.5 mG
    .accel_y = hton16(data->y * 2),
    .accel_z = hton16(data->z * 2)
  };
  prv_write_register_block(dev, ADDR_ACCEL_SAMPLE_IDX, &sample_data, sizeof(sample_data));
}

static void prv_read_hrm_data(HRMDevice *dev, HRMData *data) {
  struct PACKED {
    uint16_t led_current;
    uint8_t hrm_status;
    uint8_t bpm;
    uint8_t sqi;
  } hrm_data_regs;

  prv_read_register_block(dev, ADDR_LED_CURRENT_MSB, &hrm_data_regs, sizeof(hrm_data_regs));

  data->led_current_ua = ntohs(hrm_data_regs.led_current);
  data->hrm_status = hrm_data_regs.hrm_status;
  data->hrm_bpm = hrm_data_regs.bpm;

  if (data->hrm_status & AS7000Status_NoAccel) {
    data->hrm_quality = HRMQuality_NoAccel;
  } else if (hrm_data_regs.sqi <= AS7000SQIThreshold_Excellent) {
    data->hrm_quality = HRMQuality_Excellent;
  } else if (hrm_data_regs.sqi <= AS7000SQIThreshold_Good) {
    data->hrm_quality = HRMQuality_Good;
  } else if (hrm_data_regs.sqi <= AS7000SQIThreshold_Acceptable) {
    data->hrm_quality = HRMQuality_Acceptable;
  } else if (hrm_data_regs.sqi <= AS7000SQIThreshold_Poor) {
    data->hrm_quality = HRMQuality_Poor;
  } else if (hrm_data_regs.sqi <= AS7000SQIThreshold_Worst) {
    data->hrm_quality = HRMQuality_Worst;
  } else if (hrm_data_regs.sqi == AS7000SQIThreshold_OffWrist) {
    data->hrm_quality = HRMQuality_OffWrist;
  } else {
    data->hrm_quality = HRMQuality_NoSignal;
  }
}

// Sequence of events for handshake pulse (when in one-second burst mode):
//    - [optional] Host writes the one-second time (registers 10,11) measured for the last 20
//      samples (about one second).
//    - Host reads any data/HRV-result/LED-current, as needed (see registers [14...53])
//    - Host reads the HRM-result/SYNC-byte (registers [54...57]).
//      If not in HRM-mode, the host can just read the SYNC-byte (register 57).
//      Reading the SYNC-byte causes the AS7000 to release the handshake-signal
//      and allows deep-sleep mode (if the AS7000 is configured for this).
//      This step must be the last read for this handshake-pulse.
static void prv_handle_handshake_pulse(void *unused_data) {
  PPG_DBG("Handshake handle");

  mutex_lock(HRM->state->lock);
  if (!hrm_is_enabled(HRM)) {
    mutex_unlock(HRM->state->lock);
    return;
  }

  // We keep track of the number of handshakes so that we know when to expect samples
  const bool should_expect_samples = (HRM->state->handshake_count > WARMUP_HANDSHAKES);

  HRMData data = (HRMData) {};

  // Immediately read the PPG data. The timing constraints are pretty tight (we need to read this
  // within 30ms~ of getting the handshake or else we'll lose PPG data). The other registers can
  // be read at anytime before the next handshake, so it's ok to do this first.
  prv_read_ppg_data(HRM, &data.ppg_data);

  if (should_expect_samples) {
    interval_timer_take_sample(&s_handshake_interval_timer);
  }

  // Send the accel data out to the AS7000
  HRMAccelData *accel_data = hrm_manager_get_accel_data();
  const uint8_t num_samples = accel_data->num_samples;
  prv_write_register(HRM, ADDR_NUM_ACCEL_SAMPLES, num_samples);
  for (uint32_t i = 0; i < num_samples; ++i) {
    prv_write_accel_sample(HRM, i + 1, &accel_data->data[i]);
  }
  data.accel_data = *accel_data;
  hrm_manager_release_accel_data();

  // Read the rest of the HRM data fields.
  prv_read_hrm_data(HRM, &data);

  // Handle the clock skew register
  uint32_t average_handshake_interval_ms;
  uint32_t num_intervals = interval_timer_get(&s_handshake_interval_timer,
                                              &average_handshake_interval_ms);
  // Try to write the register frequently early on, and then every half second to accommodate
  // changes over time.
  if (num_intervals == 2 ||
      num_intervals == 10 ||
      (num_intervals % 30) == 0) {
    prv_set_host_one_second_time_register(HRM, average_handshake_interval_ms);
  }

  // Read the SYNC byte to release handshake signal and enter deep sleep mode.
  uint8_t unused;
  prv_read_register(HRM, ADDR_SYNC, &unused);


  PPG_DBG("Handshake handle done");
  HRM->state->handshake_count++;


  PROFILER_NODE_STOP(hrm_handling);
  mutex_unlock(HRM->state->lock);


  // PPG_DBG log out each PPG data sample that we recorded
  for (int i = 0; i < data.ppg_data.num_samples; i++) {
    PPG_DBG_VERBOSE("idx %-2"PRIu8" ppg %-6"PRIu16" tia %-6"PRIu16,
                    data.ppg_data.indexes[i], data.ppg_data.ppg[i], data.ppg_data.tia[i]);
  }

  hrm_manager_new_data_cb(&data);

  if (num_samples == 0 && should_expect_samples) {
    analytics_inc(ANALYTICS_DEVICE_METRIC_HRM_ACCEL_DATA_MISSING, AnalyticsClient_System);
    PBL_LOG(LOG_LEVEL_WARNING, "Falling behind: HRM got 0 accel samples");
  }

}

static void prv_as7000_interrupt_handler(bool *should_context_switch) {
  PPG_DBG("Handshake interrupt");

  PROFILER_NODE_START(hrm_handling); // Starting to respond to handshake toggle

  // Reset the watchdog counter
  s_missing_interrupt_count = 0;

  *should_context_switch = new_timer_add_work_callback_from_isr(prv_handle_handshake_pulse, NULL);
}

static void prv_interrupts_enable(HRMDevice *dev, bool enable) {
  mutex_assert_held_by_curr_task(dev->state->lock, true);
  exti_configure_pin(dev->handshake_int, ExtiTrigger_Falling, prv_as7000_interrupt_handler);
  exti_enable(dev->handshake_int);
}

static void prv_log_running_apps(HRMDevice *dev) {
  uint8_t app_ids = 0;
  if (!prv_read_register(dev, ADDR_APP_IDS, &app_ids)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to get running apps");
    return;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "Running applications:");
  if (app_ids == AS7000AppId_Idle) {
    PBL_LOG(LOG_LEVEL_DEBUG, " - None (idle)");
  } else {
    if (app_ids & AS7000AppId_Loader) {
      PBL_LOG(LOG_LEVEL_DEBUG, " - Loader");
    }
    if (app_ids & AS7000AppId_HRM) {
      PBL_LOG(LOG_LEVEL_DEBUG, " - HRM");
    }
    if (app_ids & AS7000AppId_PRV) {
      PBL_LOG(LOG_LEVEL_DEBUG, " - PRV");
    }
    if (app_ids & AS7000AppId_GSR) {
      PBL_LOG(LOG_LEVEL_DEBUG, " - GSR");
    }
    if (app_ids & AS7000AppId_NTC) {
      PBL_LOG(LOG_LEVEL_DEBUG, " - NTC");
    }
  }
}

static bool prv_get_and_log_device_info(HRMDevice *dev, AS7000InfoRecord *info,
                                        bool log_version) {
  // get the device info
  if (!prv_read_register_block(dev, ADDR_INFO_START, info, sizeof(AS7000InfoRecord))) {
    return false;
  }

  if (log_version) {
    // print out the version information
    PBL_LOG(LOG_LEVEL_INFO, "AS7000 enabled! Protocol v%" PRIu8 ".%" PRIu8
      ", SW v%" PRIu8 ".%" PRIu8 ".%" PRIu8 ", HW Rev %" PRIu8,
            info->protocol_version_major, info->protocol_version_minor,
            HRM_SW_VERSION_PART_MAJOR(info->sw_version_major),
            HRM_SW_VERSION_PART_MINOR(info->sw_version_major),
            info->sw_version_minor, info->hw_revision);
  }
  prv_log_running_apps(dev);
  return true;
}

static bool prv_is_app_running(HRMDevice *dev, AS7000AppId app) {
  uint8_t running_apps = 0;
  if (!prv_read_register(dev, ADDR_APP_IDS, &running_apps)) {
    return false;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "Apps running: 0x%"PRIx8, running_apps);
  if (app == AS7000AppId_Idle) {
    // no apps should be running
    return running_apps == AS7000AppId_Idle;
  }
  return running_apps & app;
}

//! Set the applications that should be running on the HRM.
//!
//! This commands the HRM to start or continue running any apps whose
//! flags are set, and to stop all apps whose flags are unset. Depending
//! on the firmware loaded onto the HRM, multiple apps can be run
//! concurrently by setting the logical OR of the App IDs.
static bool prv_set_running_apps(HRMDevice *dev, AS7000AppId apps) {
  return prv_write_register(dev, ADDR_APP_IDS, apps);
}

// Wait for the INT line to go low. Return true if it went low before timing out
static bool prv_wait_int_low(HRMDevice *dev) {
  const int max_attempts = 2000;
  int attempt;
  for (attempt = 0; attempt < max_attempts; attempt++) {
    if (!gpio_input_read(&dev->int_gpio)) {
      break;
    }
    system_task_watchdog_feed();
    psleep(1);
  }
  return (attempt < max_attempts);
}

// Wait for the INT line to go high. Return true if it went high before timing out
static bool prv_wait_int_high(HRMDevice *dev) {
  const int max_attempts = 300;
  int attempt;
  for (attempt = 0; attempt < max_attempts; attempt++) {
    if (gpio_input_read(&dev->int_gpio)) {
      break;
    }
    system_task_watchdog_feed();
    psleep(1);
  }
  return (attempt < max_attempts);
}

// NOTE: the caller must hold the device's state lock
static void prv_disable(HRMDevice *dev) {
  mutex_assert_held_by_curr_task(dev->state->lock, true);

  // Turn off our watchdog timer
  prv_disable_watchdog(dev);

  // Make sure interrupts are fully disabled before changing state
  prv_interrupts_enable(dev, false);
  // Put the INT pin back into a low power state that won't interfere with jtag using the pin
  gpio_analog_init(&dev->int_gpio);

  PBL_LOG(LOG_LEVEL_DEBUG, "Shutting down device.");
  switch (dev->state->enabled_state) {
    case HRMEnabledState_PoweringOn:
      new_timer_stop(dev->state->timer);
      // Delay a bit so that we don't deassert the enable GPIO while in
      // the loader and unintentionally activate force loader mode.
      psleep(LOADER_READY_MAX_DELAY_MS);
      // fallthrough
    case HRMEnabledState_Enabled:
      gpio_output_set(&dev->en_gpio, false);
      dev->state->enabled_state = HRMEnabledState_Disabled;
      break;
    case HRMEnabledState_Disabled:
      // nothing to do
      break;
    case HRMEnabledState_Uninitialized:
      // the lock isn't even created yet - should never get here
      // fallthrough
    default:
      WTF;
  }
  led_disable(LEDEnablerHRM);
  analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_HRM_ON_TIME);
}

// NOTE: the caller must hold the device's state lock
static void prv_enable(HRMDevice *dev) {
  mutex_assert_held_by_curr_task(dev->state->lock, true);
  if (dev->state->enabled_state == HRMEnabledState_Uninitialized) {
    PBL_LOG(LOG_LEVEL_ERROR, "Trying to enable HRM before initialization.");

  } else if (dev->state->enabled_state == HRMEnabledState_Disabled) {
    led_enable(LEDEnablerHRM);
    analytics_stopwatch_start(ANALYTICS_DEVICE_METRIC_HRM_ON_TIME, AnalyticsClient_System);

    // Enable the device and schedule a timer callback for when we can start communicating with it.
    gpio_output_set(&dev->en_gpio, true);
    dev->state->enabled_state = HRMEnabledState_PoweringOn;
    dev->state->handshake_count = 0;
    new_timer_start(dev->state->timer, NORMAL_BOOT_DELAY_MS, prv_enable_timer_cb, (void *)dev,
                    0 /* flags */);

    interval_timer_init(&s_handshake_interval_timer, 900, 1100, 8);

    PBL_LOG(LOG_LEVEL_DEBUG, "Enabling AS7000...");
  }
}

// This system task callback is triggered by the watchdog interrupt handler when we detect
// a frozen sensor
static void prv_watchdog_timer_system_cb(void *data) {
  HRMDevice *dev = (HRMDevice *)data;
  mutex_lock(dev->state->lock);
  if (dev->state->enabled_state != HRMEnabledState_Enabled) {
    goto exit;
  }

  // If we have gone too long without getting an interrupt, let's reset the device
  if (s_missing_interrupt_count >= AS7000_MAX_WATCHDOG_INTERRUPTS) {
    PBL_LOG(LOG_LEVEL_ERROR, "Watchdog logic detected frozen sensor. Resetting now.");
    analytics_inc(ANALYTICS_DEVICE_METRIC_HRM_WATCHDOG_TIMEOUT, AnalyticsClient_System);
    prv_disable(dev);
    psleep(SHUT_DOWN_DELAY_MS);
    prv_enable(dev);
  }
exit:
  mutex_unlock(dev->state->lock);
}

// This regular timer callback executes once a second. It is part of the watchdog logic used to
// detect if the sensor becomes unresponsive.
static void prv_watchdog_timer_cb(void *data) {
  HRMDevice *dev = (HRMDevice *)data;
  if (++s_missing_interrupt_count >= AS7000_MAX_WATCHDOG_INTERRUPTS) {
    system_task_add_callback(prv_watchdog_timer_system_cb, (void *)dev);
  }
  if (s_missing_interrupt_count > 1) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Missing interrupt count: %"PRIu8" ", s_missing_interrupt_count);
  }
}

// Enable the watchdog timer. This gets enabled when we enable the sensor and detects if
// the sensor stops generating interrupts.
static void prv_enable_watchdog(HRMDevice *dev) {
  mutex_assert_held_by_curr_task(dev->state->lock, true);
  s_as7000_watchdog_timer = (RegularTimerInfo) {
    .cb = prv_watchdog_timer_cb,
    .cb_data = (void *)dev,
  };
  s_missing_interrupt_count = 0;
  regular_timer_add_seconds_callback(&s_as7000_watchdog_timer);
}

static void prv_disable_watchdog(HRMDevice *dev) {
  mutex_assert_held_by_curr_task(dev->state->lock, true);
  regular_timer_remove_callback(&s_as7000_watchdog_timer);
  s_missing_interrupt_count = 0;
}

static bool prv_start_loader(HRMDevice *dev) {
  // check if the loader is already running
  if (!prv_is_app_running(dev, AS7000AppId_Loader)) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Switching to loader");
    // we need to start the loader
    if (!prv_set_running_apps(dev, AS7000AppId_Loader)) {
      return false;
    }
    psleep(35);

    // make sure the loader is running
    if (!prv_is_app_running(dev, AS7000AppId_Loader)) {
      return false;
    }
  }
  prv_log_running_apps(dev);
  return true;
}

static uint64_t prv_get_time_ms(void) {
  time_t time_s;
  uint16_t time_ms;
  rtc_get_time_ms(&time_s, &time_ms);
  return ((uint64_t)time_s) * 1000 + time_ms;
}

static bool prv_wait_for_loader_ready(HRMDevice *dev) {
  uint64_t end_time_ms = prv_get_time_ms() + LOADER_READY_MAX_DELAY_MS;

  do {
    uint8_t status = 0;
    if (!prv_read_register(dev, ADDR_LOADER_STATUS, &status)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed reading status");
      return false;
    }

    if (status == AS7000LoaderStatus_Ready) {
      // ready
      return true;
    } else if ((status != AS7000LoaderStatus_Busy1) && (status != AS7000LoaderStatus_Busy2)) {
      // error
      PBL_LOG(LOG_LEVEL_ERROR, "Error status: %"PRIx8, status);
      return false;
    }
    psleep(1);
  } while (prv_get_time_ms() < end_time_ms);

  PBL_LOG(LOG_LEVEL_ERROR, "Timed out waiting for the loader to be ready!");
  return false;
}

static bool prv_flash_fw(HRMDevice *dev) {
  // switch to the loader
  if (!prv_start_loader(dev)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to start loader");
    return false;
  }

  // wait for the loader to be ready
  if (!prv_wait_for_loader_ready(dev)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Loader not ready");
    return false;
  }

  const uint32_t image_length =
    resource_size(SYSTEM_APP, RESOURCE_ID_AS7000_FW_IMAGE);
  PBL_ASSERTN(image_length);
  PBL_LOG(LOG_LEVEL_DEBUG,
          "Loading FW image (%"PRIu32" bytes encoded)", image_length);
  // Skip over the image header.
  uint32_t cursor = sizeof(AS7000FWUpdateHeader);
  while (cursor < image_length) {
    // Make sure we can load enough data for a valid segment. There is
    // always at least one data byte in each segment, so there must be
    // strictly more data to read past the end of the header.
    PBL_ASSERTN((image_length - cursor) > sizeof(AS7000FWSegmentHeader));
    // Read the header.
    AS7000FWSegmentHeader segment_header;
    if (resource_load_byte_range_system(
          SYSTEM_APP, RESOURCE_ID_AS7000_FW_IMAGE, cursor,
          (uint8_t *)&segment_header, sizeof(segment_header)) !=
        sizeof(segment_header)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to read FW image! "
              "(segment header @ 0x%" PRIx32 ")", cursor);
      return false;
    }
    cursor += sizeof(segment_header);
    // Write all the data bytes in the segment to the HRM.
    uint16_t write_address = segment_header.address;
    uint32_t bytes_remaining = segment_header.len_minus_1 + 1;
    while (bytes_remaining) {
      uint8_t chunk[MAX_HEX_DATA_BYTES];
      const size_t load_length = MIN(MAX_HEX_DATA_BYTES, bytes_remaining);
      if (resource_load_byte_range_system(
            SYSTEM_APP, RESOURCE_ID_AS7000_FW_IMAGE, cursor, chunk, load_length)
          != load_length) {
        PBL_LOG(LOG_LEVEL_ERROR, "Failed to read FW image! "
                "(segment data @ 0x%" PRIx32 ")", cursor);
        return false;
      }

      // Encode the chunk into an Intel HEX record and send it to the
      // AS7000 loader.
      uint8_t data_record[IHEX_RECORD_LENGTH(MAX_HEX_DATA_BYTES)];
      ihex_encode(data_record, IHEX_TYPE_DATA, write_address,
                  chunk, load_length);
      if (!prv_write_register_block(dev, ADDR_LOADER_STATUS, data_record,
                                    IHEX_RECORD_LENGTH(load_length))) {
        PBL_LOG(LOG_LEVEL_ERROR, "Failed to write hex record");
        return false;
      }

      // Wait for the loader to be ready, indicating that the last
      // record was successfully written.
      if (!prv_wait_for_loader_ready(dev)) {
        PBL_LOG(LOG_LEVEL_ERROR, "Loader not ready");
        return false;
      }

      system_task_watchdog_feed();

      cursor += load_length;
      write_address += load_length;
      bytes_remaining -= load_length;
    }
  }

  // Write the EOF record, telling the loader that the image has been
  // fully written.
  uint8_t eof_record[IHEX_RECORD_LENGTH(0)];
  ihex_encode(eof_record, IHEX_TYPE_EOF, 0, NULL, 0);
  if (!prv_write_register_block(dev, ADDR_LOADER_STATUS,
                                eof_record, IHEX_RECORD_LENGTH(0))) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to write EOF record");
    return false;
  }

  return true;
}

static bool prv_set_accel_sample_frequency(HRMDevice *dev, uint16_t freq) {
  const uint8_t msb = (freq >> 8) & 0xff;
  const uint8_t lsb = freq & 0xff;
  return prv_write_register(dev, ADDR_ACCEL_SAMPLE_FREQ_MSB, msb)
      && prv_write_register(dev, ADDR_ACCEL_SAMPLE_FREQ_LSB, lsb);
}

static void prv_enable_system_task_cb(void *context) {
  HRMDevice *dev = context;
  mutex_lock(dev->state->lock);
  if (dev->state->enabled_state == HRMEnabledState_Disabled) {
    // Enable was cancelled before this callback fired.
    goto done;
  } else if (dev->state->enabled_state != HRMEnabledState_PoweringOn) {
    PBL_LOG(LOG_LEVEL_ERROR, "Enable KernelBG callback fired while HRM was in "
            "an unexpected state: %u", (unsigned int)dev->state->enabled_state);
    WTF;
  }

  AS7000InfoRecord info;
  if (!prv_get_and_log_device_info(dev, &info, false /* log_version */)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to query AS7000 device info");
    goto failed;
  }

  if (info.application_id == AS7000AppId_Loader) {
    // This shouldn't happen. The application firmware should have been
    // flashed during boot.
    PBL_LOG(LOG_LEVEL_ERROR,
            "AS7000 booted into loader! Something is very wrong.");
    goto failed;
  }

  // check that we can communicate with this chip
  if (info.protocol_version_major != EXPECTED_PROTOCOL_VERSION_MAJOR) {
    // we don't know how to talk with this chip, so bail
    PBL_LOG(LOG_LEVEL_ERROR, "Unexpected protocol version!");
    goto failed;
  }

  if (info.application_id != AS7000AppId_Idle) {
    PBL_LOG(LOG_LEVEL_ERROR,
            "Unexpected application running: 0x%" PRIx8, info.application_id);
    goto failed;
  }

  // the INT line should be low
  if (gpio_input_read(&dev->int_gpio)) {
    PBL_LOG(LOG_LEVEL_ERROR, "INT line is not low!");
    goto failed;
  }

  // Set the accelerometer sample frequency
  PBL_LOG(LOG_LEVEL_DEBUG, "Setting accel frequency");
  PBL_ASSERTN(HRM_MANAGER_ACCEL_RATE_MILLIHZ >= 10000 && HRM_MANAGER_ACCEL_RATE_MILLIHZ <= 20000);
  if (!prv_set_accel_sample_frequency(dev, HRM_MANAGER_ACCEL_RATE_MILLIHZ)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set accel frequency");
    goto failed;
  }

  // Set the presence detection threshold
  uint8_t pres_detect_thrsh;
  WatchInfoColor model_color = mfg_info_get_watch_color();
  switch (model_color) {
    case WATCH_INFO_COLOR_PEBBLE_2_HR_BLACK:
    case WATCH_INFO_COLOR_PEBBLE_2_HR_FLAME:
      pres_detect_thrsh = PRES_DETECT_THRSH_BLACK;
      break;
    case WATCH_INFO_COLOR_PEBBLE_2_HR_WHITE:
    case WATCH_INFO_COLOR_PEBBLE_2_HR_LIME:
    case WATCH_INFO_COLOR_PEBBLE_2_HR_AQUA:
      pres_detect_thrsh = PRES_DETECT_THRSH_WHITE;
      break;
    default:
      pres_detect_thrsh = 1;
      break;
  }
  if (!prv_write_register(dev, ADDR_PRES_DETECT_THRSH, pres_detect_thrsh)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to set presence detection threshold");
    goto failed;
  }

  // start the HRM app
  PBL_LOG(LOG_LEVEL_DEBUG, "Starting HRM app");
  if (!prv_set_running_apps(dev, AS7000AppId_HRM)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to start HRM app!");
    goto failed;
  }

  // Configure the int_gpio pin only when we're going to use it, as this pin is shared with
  // the jtag pins and therefore can cause issues when flashing firmwares onto bigboards.
  gpio_input_init_pull_up_down(&dev->int_gpio, GPIO_PuPd_UP);

  // wait for the INT line to go high indicating the Idle app has ended
  if (!prv_wait_int_high(dev)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Timed-out waiting for the Idle app to end but we "
            "probably just missed it");
    // TODO: The line only goes high for a few ms. If there is any kind of context switch while we
    // wait for the line to go high we will miss this. Let's fix this the right way in PBL-41812
    // (check for this change via an ISR) for 4.2 but just go with the smallest change for 4.1
  }

  // wait for the INT line to go low indicating the HRM app is ready
  if (!prv_wait_int_low(dev)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Timed-out waiting for the HRM app to be ready");
    goto failed;
  }

  // get the running apps (also triggers the app to start)
  prv_log_running_apps(dev);

  // HRM app is ready, enable handshake interrupts
  prv_interrupts_enable(dev, true);

  // We are now fully enabled
  dev->state->enabled_state = HRMEnabledState_Enabled;

  // Enable the watchdog
  prv_enable_watchdog(dev);

  goto done;

failed:
  prv_disable(dev);
done:
  mutex_unlock(dev->state->lock);
}

static void prv_enable_timer_cb(void *context) {
  system_task_add_callback(prv_enable_system_task_cb, context);
}

void hrm_init(HRMDevice *dev) {
  PBL_ASSERTN(dev->state->enabled_state == HRMEnabledState_Uninitialized);

  dev->state->lock = mutex_create();
  dev->state->timer = new_timer_create();
  dev->state->enabled_state = HRMEnabledState_Disabled;

  // Boot up the HRM so that we can read off the firmware version to see
  // if it needs to be updated.

  // First, read the version from the firmware update resource.
  const uint32_t update_length = resource_size(
      SYSTEM_APP, RESOURCE_ID_AS7000_FW_IMAGE);
  if (update_length == 0) {
    // We don't have a firmware to write so there's no point in booting
    // the HRM.
    PBL_LOG(LOG_LEVEL_DEBUG, "No HRM FW update available");
    return;
  }

  AS7000FWUpdateHeader image_header;
  if (resource_load_byte_range_system(
        SYSTEM_APP, RESOURCE_ID_AS7000_FW_IMAGE, 0, (uint8_t *)&image_header,
        sizeof(image_header)) != sizeof(image_header)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read HRM FW image header!");
    return;
  }
  PBL_LOG(LOG_LEVEL_DEBUG, "FW update image is v%" PRIu8 ".%" PRIu8 ".%" PRIu8,
          HRM_SW_VERSION_PART_MAJOR(image_header.sw_version_major),
          HRM_SW_VERSION_PART_MINOR(image_header.sw_version_major),
          image_header.sw_version_minor);

  // Now that we know what version the image is, actually boot up the
  // HRM so we can read off the version.

  PBL_LOG(LOG_LEVEL_DEBUG, "Booting AS7000...");

  gpio_output_init(&dev->en_gpio, GPIO_OType_PP, GPIO_Speed_2MHz);
#if HRM_FORCE_FLASH
  // Force the HRM into loader mode which will cause the firmware to be
  // reflashed on every boot. If the HRM is loaded with a broken
  // firmware which doesn't enter standby when the enable pin is high,
  // the board will need to be power-cycled (entering standby/shutdown
  // is sufficient) in order to get force-flashing to succeed.
  gpio_output_set(&dev->en_gpio, false);
  psleep(50);
  gpio_output_set(&dev->en_gpio, true);
  psleep(20);
  gpio_output_set(&dev->en_gpio, false);
  psleep(20);
#else
  gpio_output_set(&dev->en_gpio, true);
  psleep(NORMAL_BOOT_DELAY_MS);
#endif

  AS7000InfoRecord hrm_info;
  if (!prv_get_and_log_device_info(dev, &hrm_info, true /* log_version */)) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read AS7000 version info!");
    goto cleanup;
  }

  if (hrm_info.application_id == AS7000AppId_Loader ||
      hrm_info.sw_version_major != image_header.sw_version_major ||
      hrm_info.sw_version_minor != image_header.sw_version_minor) {
    // We technically could leave the firmware on the HRM alone if the
    // minor version in the chip is newer than in the update image, but
    // for sanity's sake let's always make sure the HRM firmware is in
    // sync with the version shipped with the Pebble firmware.
    PBL_LOG(LOG_LEVEL_DEBUG, "AS7000 firmware version mismatch. Flashing...");
    if (!prv_flash_fw(dev)) {
      PBL_LOG(LOG_LEVEL_ERROR, "Failed to flash firmware");
      goto cleanup;
    }
    // We need to wait for the HRM to reboot into the application before
    // releasing the enable GPIO. If the loader sees the GPIO released
    // during boot, it will activate "force loader mode" and fall back
    // into the loader. Since we're waiting anyway, we might as well
    // query the version info again to make sure the update took.
    PBL_LOG(LOG_LEVEL_DEBUG, "Firmware flashed! Waiting for reboot...");
    gpio_output_set(&dev->en_gpio, true);
    psleep(LOADER_REBOOT_DELAY_MS);
    if (!prv_get_and_log_device_info(dev, &hrm_info, true /* log_version */)) {
      PBL_LOG(LOG_LEVEL_ERROR,
              "Failed to read AS7000 version info after flashing!");
      goto cleanup;
    }
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "AS7000 firmware is up to date.");
  }

cleanup:
  // At this point the HRM should either be booted and running the
  // application firmware, at which point deasserting the enable GPIO
  // will signal it to shut down, or the firmware update failed and the
  // loader is running, where deasserting the GPIO will not do much.
  gpio_output_set(&dev->en_gpio, false);
}

void hrm_enable(HRMDevice *dev) {
  if (!dev->state->lock) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Not an HRM Device.");
    return;
  }

  mutex_lock(dev->state->lock);
  prv_enable(dev);
  mutex_unlock(dev->state->lock);
}

void hrm_disable(HRMDevice *dev) {
  if (!dev->state->lock) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Not an HRM Device.");
    return;
  }

  mutex_lock(dev->state->lock);
  prv_disable(dev);
  mutex_unlock(dev->state->lock);
}

bool hrm_is_enabled(HRMDevice *dev) {
  return (dev->state->enabled_state == HRMEnabledState_Enabled
         || dev->state->enabled_state == HRMEnabledState_PoweringOn);
}

void as7000_get_version_info(HRMDevice *dev, AS7000InfoRecord *info_out) {
  if (!dev->state->lock) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Not an HRM Device.");
    return;
  }

  mutex_lock(dev->state->lock);
  if (!prv_get_and_log_device_info(dev, info_out, true /* log_version */)) {
    PBL_LOG(LOG_LEVEL_WARNING, "Failed to read AS7000 version info");
  }
  mutex_unlock(dev->state->lock);
}

// Prompt Commands
// ===============

#include "console/prompt.h"
#include <string.h>

void command_hrm_wipe(void) {
  // HEX records to write 0xFFFFFFFF to the magic number region.
  const char *erase_magic_record = ":047FFC00FFFFFFFF85";
  const char *eof_record = ":00000001FF";

  mutex_lock(HRM->state->lock);
  gpio_output_set(&HRM->en_gpio, true);
  psleep(NORMAL_BOOT_DELAY_MS);

  bool success = prv_start_loader(HRM) &&
                 prv_wait_for_loader_ready(HRM) &&
                 prv_write_register_block(HRM, ADDR_LOADER_STATUS,
                                          erase_magic_record,
                                          strlen(erase_magic_record)) &&
                 prv_wait_for_loader_ready(HRM) &&
                 prv_write_register_block(HRM, ADDR_LOADER_STATUS,
                                          eof_record, strlen(eof_record)) &&
                 prv_wait_for_loader_ready(HRM);

  gpio_output_set(&HRM->en_gpio, false);
  mutex_unlock(HRM->state->lock);

  prompt_send_response(success? "HRM Firmware invalidated" : "ERROR");
}

// Simulate a frozen sensor for testing the watchdog recovery logic
void command_hrm_freeze(void) {
  HRMDevice *dev = HRM;
  mutex_lock(dev->state->lock);
  if (dev->state->enabled_state == HRMEnabledState_Enabled) {
    prv_interrupts_enable(dev, false);
    gpio_analog_init(&dev->int_gpio);
    led_disable(LEDEnablerHRM);
  }
  mutex_unlock(dev->state->lock);
}
