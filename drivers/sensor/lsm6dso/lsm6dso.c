/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/devicetree.h>
#include <pbl/drivers/gpio.h>
#include <pbl/drivers/i2c.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/lsm6dso/lsm6dso.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(driver_sensor, CONFIG_DRIVER_SENSOR_LOG_LEVEL);

// The single instance, for interrupt and timer contexts
static const struct pbl_lsm6dso *s_dev;

// Implementation notes:
//
// - Frames are reported in the sensor axes; the class applies the mount matrix
//   and the rotation.
// - Accelerometer and gyroscope words share the FIFO, each tagged with its
//   sensor. A FIFO read is split by tag into one batch per sensor; frame times
//   assume the words of both sensors come in evenly spaced.
// - Reads return the output registers of a running sensor, otherwise a
//   single-shot measurement is performed.
// - The accelerometer uses ultra-low-power mode (XL_ULP_EN) while the
//   gyroscope is off and its ODR is 208 Hz or less, as ULP requires. XL_ULP_EN
//   is only changed with the accelerometer powered down.
// - ODRs range from 12.5 to 833 Hz.
// - Shake detection uses 12.5Hz when no active sampling is ongoing.
// - Wake-up duration absolute time depends on the ODR, a parameter that can
//   be changed depending on the sampling interval configuration. Value is NOT
//   adjusted automatically when ODR changes, so it is possible to notice
//   sensitivity changes when changing sampling interval.
// - The FIFO is drained from the INT1 interrupt with asynchronous I2C
//   transfers, timestamped from the edge, and batches are submitted to the
//   class from there. Shake events, recoveries and the watchdog run on the
//   high priority work queue, which blocks the drains while it reconfigures.
// - INT1 mixes level (FIFO threshold) and latched (wake-up) sources on a
//   rising-edge interrupt, so a pad latched HIGH yields no further edges: shake
//   events die and the pending interrupt blocks deep sleep. The drain and the
//   work handler re-check the pad after servicing; a watchdog recovers the
//   stream (FIFO cadence while sampling, fixed-rate pad poll in shake-only
//   mode).

// Time to wait after reset (us)
#define LSM6DSO_RESET_TIME_US 5

// DRDY polling parameters for single-shot peeks
#define LSM6DSO_DRDY_POLL_DELAY_MS   (5)   /* ms between data-ready polls */
#define LSM6DSO_DRDY_POLL_TIMEOUT_MS (100) /* max wait (~5x 20ms at 52Hz ODR) */

// FIFO threshold periods without a FIFO read before the stream is considered
// stalled (>1x to tolerate scheduling jitter), and threshold lower bound
#define LSM6DSO_STALL_MARGIN 2U
#define LSM6DSO_STALL_MIN_MS 1000U

// Max buffer-sized reads when draining the FIFO
#define LSM6DSO_DRAIN_CHUNKS_MAX 4U

// Longest wait for an asynchronous drain in flight, two transfers timing out
#define LSM6DSO_DRAIN_WAIT_MS 2500U

// INT1 watchdog poll rate in shake-only mode (no FIFO cadence to track)
#define LSM6DSO_SHAKE_WDT_PERIOD_S 5U

// Consecutive stuck-high watchdog passes before a full stream recovery
#define LSM6DSO_SHAKE_STUCK_PASSES_MAX 3U

// Scale range for 16-bit two's complement samples
#define LSM6DSO_S16_SCALE_RANGE 32768

// FIFO word tags (TAG_SENSOR, the 5 MSBs of the tag byte)
#define LSM6DSO_FIFO_TAG_SHIFT   3U
#define LSM6DSO_FIFO_TAG_GYRO_NC 0x01U
#define LSM6DSO_FIFO_TAG_XL_NC   0x02U

// Highest ODR code allowed in ultra-low-power mode (208 Hz)
#define LSM6DSO_ODR_ULP_MAX 5U

// Gyroscope turn-on time before the first valid sample of a single-shot read
#define LSM6DSO_GYRO_TURN_ON_MS 80U

// Registers
#define LSM6DSO_FIFO_CTRL1        0x07U
#define LSM6DSO_FIFO_CTRL2        0x08U
#define LSM6DSO_FIFO_CTRL3        0x09U
#define LSM6DSO_FIFO_CTRL4        0x0AU
#define LSM6DSO_INT1_CTRL         0x0DU
#define LSM6DSO_WHO_AM_I          0x0FU
#define LSM6DSO_CTRL1_XL          0x10U
#define LSM6DSO_CTRL2_G           0x11U
#define LSM6DSO_CTRL3_C           0x12U
#define LSM6DSO_CTRL5_C           0x14U
#define LSM6DSO_CTRL9_XL          0x18U
#define LSM6DSO_ALL_INT_SRC       0x1AU
#define LSM6DSO_WAKE_UP_SRC       0x1BU
#define LSM6DSO_STATUS_REG        0x1EU
#define LSM6DSO_OUTX_L_G          0x22U
#define LSM6DSO_OUTX_L_A          0x28U
#define LSM6DSO_FIFO_STATUS1      0x3AU
#define LSM6DSO_FIFO_STATUS2      0x3BU
#define LSM6DSO_TAP_CFG0          0x56U
#define LSM6DSO_TAP_CFG2          0x58U
#define LSM6DSO_WAKE_UP_THS       0x5BU
#define LSM6DSO_WAKE_UP_DUR       0x5CU
#define LSM6DSO_MD1_CFG           0x5EU
#define LSM6DSO_FIFO_DATA_OUT_TAG 0x78U

// WHO_AM_I value
#define LSM6DSO_WHO_AM_I_VAL 0x6CU

// CTRL1_XL fields
#define LSM6DSO_CTRL1_XL_ODR_MASK PBL_GENMASK(7, 4)
#define LSM6DSO_CTRL1_XL_ODR_OFF  0x0U
#define LSM6DSO_CTRL1_XL_ODR_52HZ 0x3U
#define LSM6DSO_CTRL1_XL_FS_MASK  PBL_GENMASK(3, 2)
#define LSM6DSO_CTRL1_XL_FS_2G    0x0U
#define LSM6DSO_CTRL1_XL_FS_16G   0x1U
#define LSM6DSO_CTRL1_XL_FS_4G    0x2U
#define LSM6DSO_CTRL1_XL_FS_8G    0x3U

// CTRL2_G fields
#define LSM6DSO_CTRL2_G_ODR_MASK PBL_GENMASK(7, 4)
#define LSM6DSO_CTRL2_G_FS_MASK  PBL_GENMASK(3, 2)
#define LSM6DSO_CTRL2_G_FS_125   PBL_BIT(1)

// CTRL3_C fields
#define LSM6DSO_CTRL3_C_SW_RESET PBL_BIT(0)
#define LSM6DSO_CTRL3_C_IF_INC   PBL_BIT(2)
#define LSM6DSO_CTRL3_C_BDU      PBL_BIT(6)

// CTRL5_C fields
#define LSM6DSO_CTRL5_C_XL_ULP_EN PBL_BIT(7)

// CTRL9_XL fields
#define LSM6DSO_CTRL9_XL_I3C_DISABLE PBL_BIT(1)

// INT1_CTRL fields
#define LSM6DSO_INT1_CTRL_DRDY_XL  PBL_BIT(0)
#define LSM6DSO_INT1_CTRL_FIFO_TH  PBL_BIT(3)
#define LSM6DSO_INT1_CTRL_FIFO_OVR PBL_BIT(4)

// WAKE_UP_SRC fields
#define LSM6DSO_WAKE_UP_SRC_WU_IA              PBL_BIT(3)
#define LSM6DSO_WAKE_UP_SRC_AXIS_WU(chip_axis) PBL_BIT(2U - (chip_axis))

// STATUS_REG fields
#define LSM6DSO_STATUS_REG_XLDA PBL_BIT(0)
#define LSM6DSO_STATUS_REG_GDA  PBL_BIT(1)

// FIFO_CTRL2 fields
#define LSM6DSO_FIFO_CTRL2_WTM8 PBL_BIT(0)

// FIFO_CTRL3 fields
#define LSM6DSO_FIFO_CTRL3_BDR_XL_MASK PBL_GENMASK(3, 0)
#define LSM6DSO_FIFO_CTRL3_BDR_GY_MASK PBL_GENMASK(7, 4)

// FIFO_CTRL4 fields
#define LSM6DSO_FIFO_CTRL4_MODE_MASK   PBL_GENMASK(2, 0)
#define LSM6DSO_FIFO_CTRL4_MODE_BYPASS 0x0U
#define LSM6DSO_FIFO_CTRL4_MODE_STREAM 0x6U

// FIFO_STATUS fields
#define LSM6DSO_FIFO_STATUS2_DIFF_HI_MASK PBL_GENMASK(1, 0)
#define LSM6DSO_FIFO_STATUS2_FIFO_OVR_IA  PBL_BIT(6)
#define LSM6DSO_FIFO_STATUS2_FIFO_WTM_IA  PBL_BIT(7)

// TAP_CFG0 fields (slope_fds left at 0 to select the slope filter for wake-up)
#define LSM6DSO_TAP_CFG0_LIR             PBL_BIT(0)
#define LSM6DSO_TAP_CFG0_INT_CLR_ON_READ PBL_BIT(6)

// TAP_CFG2 fields
#define LSM6DSO_TAP_CFG2_INTERRUPTS_ENABLE PBL_BIT(7)

// WAKE_UP_THS fields
#define LSM6DSO_WAKE_UP_THS_WK_THS_MASK PBL_GENMASK(5, 0)

// WAKE_UP_DUR fields
#define LSM6DSO_WAKE_UP_DUR_WAKE_DUR_MASK PBL_GENMASK(6, 5)

// MD1_CFG fields
#define LSM6DSO_MD1_CFG_INT1_WU PBL_BIT(5)

// Raw counts to micro-g
#define LSM6DSO_UG_NUM (CONFIG_ACCEL_LSM6DSO_SCALE_MG * 1000)

// Gyroscope sensitivity, in micro-dps per LSB
#if CONFIG_ACCEL_LSM6DSO_GYRO_SCALE_DPS == 125
#define LSM6DSO_GYRO_UDPS 4375
#define LSM6DSO_GYRO_FS   LSM6DSO_CTRL2_G_FS_125
#elif CONFIG_ACCEL_LSM6DSO_GYRO_SCALE_DPS == 250
#define LSM6DSO_GYRO_UDPS 8750
#define LSM6DSO_GYRO_FS   PBL_FIELD_PREP(LSM6DSO_CTRL2_G_FS_MASK, 0U)
#elif CONFIG_ACCEL_LSM6DSO_GYRO_SCALE_DPS == 500
#define LSM6DSO_GYRO_UDPS 17500
#define LSM6DSO_GYRO_FS   PBL_FIELD_PREP(LSM6DSO_CTRL2_G_FS_MASK, 1U)
#elif CONFIG_ACCEL_LSM6DSO_GYRO_SCALE_DPS == 1000
#define LSM6DSO_GYRO_UDPS 35000
#define LSM6DSO_GYRO_FS   PBL_FIELD_PREP(LSM6DSO_CTRL2_G_FS_MASK, 2U)
#else
#define LSM6DSO_GYRO_UDPS 70000
#define LSM6DSO_GYRO_FS   PBL_FIELD_PREP(LSM6DSO_CTRL2_G_FS_MASK, 3U)
#endif

// Channels of a sample, both in the output registers and past the tag of a FIFO word
static const struct pbl_sensor_chan s_xl_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, LSM6DSO_UG_NUM,
                  LSM6DSO_S16_SCALE_RANGE, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, LSM6DSO_UG_NUM,
                  LSM6DSO_S16_SCALE_RANGE, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_ACCEL, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, LSM6DSO_UG_NUM,
                  LSM6DSO_S16_SCALE_RANGE, 0),
};

static const struct pbl_sensor_chan s_gyro_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_X, 0, PBL_SENSOR_FMT_S16_LE, LSM6DSO_GYRO_UDPS,
                  1000, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_Y, 2, PBL_SENSOR_FMT_S16_LE, LSM6DSO_GYRO_UDPS,
                  1000, 0),
  PBL_SENSOR_CHAN(PBL_SENSOR_GYRO, PBL_SENSOR_AXIS_Z, 4, PBL_SENSOR_FMT_S16_LE, LSM6DSO_GYRO_UDPS,
                  1000, 0),
};

// Interval of each ODR and batch data rate code from 1 (12.5 Hz) up
static const uint32_t s_odr_us[] = {80000U, 38461U, 19230U, 9615U, 4807U, 2403U, 1200U};

////////////////////////////////////////////////////////////////////////////////
// Private
////////////////////////////////////////////////////////////////////////////////

static bool prv_lsm6dso_write(uint8_t reg, const uint8_t *data, uint16_t len) {
  return pbl_i2c_burst_write(&s_dev->i2c, reg, data, len) == 0;
}

static bool prv_lsm6dso_read(uint8_t reg, uint8_t *data, uint16_t len) {
  return pbl_i2c_burst_read(&s_dev->i2c, reg, data, len) == 0;
}

static bool prv_lsm6dso_read_fifo(uint8_t *buf, uint16_t samples) {
  uint8_t reg = LSM6DSO_FIFO_DATA_OUT_TAG;
  struct pbl_i2c_msg msgs[] = {
    {.buf = &reg, .len = 1U, .flags = PBL_I2C_MSG_WRITE},
    {.buf = buf,
     .len = samples * LSM6DSO_FIFO_WORD_SIZE_BYTES,
     .flags = PBL_I2C_MSG_READ | PBL_I2C_MSG_DMA_SAFE},
  };

  return pbl_i2c_transfer(&s_dev->i2c, msgs, PBL_ARRAY_SIZE(msgs)) == 0;
}

static uint8_t prv_fs_bits(void) {
  switch (CONFIG_ACCEL_LSM6DSO_SCALE_MG) {
    case 2000U:
      return PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_FS_MASK, LSM6DSO_CTRL1_XL_FS_2G);
    case 4000U:
      return PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_FS_MASK, LSM6DSO_CTRL1_XL_FS_4G);
    case 8000U:
      return PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_FS_MASK, LSM6DSO_CTRL1_XL_FS_8G);
    case 16000U:
      return PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_FS_MASK, LSM6DSO_CTRL1_XL_FS_16G);
    default:
      return PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_FS_MASK, LSM6DSO_CTRL1_XL_FS_2G);
  }
}

//! Split the words read into buf, a buffer from pbl_sensor_buf_alloc(), by sensor and submit
//! one batch per sensor. The last word was queued at t_last_us.
static void prv_lsm6dso_process_samples(uint8_t *buf, uint16_t num_words, uint64_t t_last_us) {
  const struct pbl_lsm6dso_state *state = s_dev->state;
  uint16_t num_xl = 0U;
  uint16_t num_gyro = 0U;
  uint16_t last_xl = 0U;
  uint16_t last_gyro = 0U;
  uint8_t *gyro = nullptr;
  bool split = false;

  for (uint16_t i = 0U; i < num_words; i++) {
    uint8_t tag = buf[i * LSM6DSO_FIFO_WORD_SIZE_BYTES] >> LSM6DSO_FIFO_TAG_SHIFT;

    if (tag == LSM6DSO_FIFO_TAG_XL_NC) {
      num_xl++;
      last_xl = i;
      continue;
    }

    split = true;
    if (tag == LSM6DSO_FIFO_TAG_GYRO_NC) {
      num_gyro++;
      last_gyro = i;
    }
  }

  if (num_gyro > 0U) {
    gyro = pbl_sensor_buf_alloc(&s_dev->sensor, nullptr);
    if (gyro == nullptr) {
      num_gyro = 0U;
    }
  }

  // Pack the accelerometer samples in place, behind the words still to be read
  if (split) {
    uint16_t xl = 0U;
    uint16_t g = 0U;

    for (uint16_t i = 0U; i < num_words; i++) {
      const uint8_t *word = &buf[i * LSM6DSO_FIFO_WORD_SIZE_BYTES];
      uint8_t tag = word[0] >> LSM6DSO_FIFO_TAG_SHIFT;

      if (tag == LSM6DSO_FIFO_TAG_XL_NC) {
        memmove(&buf[xl++ * LSM6DSO_SAMPLE_SIZE_BYTES], &word[1], LSM6DSO_SAMPLE_SIZE_BYTES);
      } else if (tag == LSM6DSO_FIFO_TAG_GYRO_NC && gyro != nullptr) {
        memcpy(&gyro[g++ * LSM6DSO_SAMPLE_SIZE_BYTES], &word[1], LSM6DSO_SAMPLE_SIZE_BYTES);
      }
    }
  }

  if (num_xl > 0U) {
    pbl_sensor_submit(&s_dev->sensor,
                      &(const struct pbl_sensor_batch){
                        .data = split ? buf : &buf[1],
                        .chans = s_xl_chans,
                        .num_chans = PBL_ARRAY_SIZE(s_xl_chans),
                        .stride = split ? LSM6DSO_SAMPLE_SIZE_BYTES : LSM6DSO_FIFO_WORD_SIZE_BYTES,
                        .num_frames = num_xl,
                        .timestamp_us = t_last_us - (uint64_t)(num_words - 1U - last_xl) *
                                                        state->word_interval_us,
                        .interval_us = state->fifo_xl_us,
                      });
  } else {
    pbl_sensor_buf_free(&s_dev->sensor, buf);
  }

  if (num_gyro > 0U) {
    pbl_sensor_submit(&s_dev->sensor,
                      &(const struct pbl_sensor_batch){
                        .data = gyro,
                        .chans = s_gyro_chans,
                        .num_chans = PBL_ARRAY_SIZE(s_gyro_chans),
                        .stride = LSM6DSO_SAMPLE_SIZE_BYTES,
                        .num_frames = num_gyro,
                        .timestamp_us = t_last_us - (uint64_t)(num_words - 1U - last_gyro) *
                                                        state->word_interval_us,
                        .interval_us = state->fifo_gyro_us,
                      });
  }
}

//! Buffer for a FIFO read, or NULL if the listener still holds all of them
static uint8_t *prv_alloc_buf(void) {
  uint8_t *buf = pbl_sensor_buf_alloc(&s_dev->sensor, nullptr);

  if (buf == nullptr) {
    PBL_LOG_WRN("No free buffer, leaving samples in the FIFO");
  }

  return buf;
}

//! ODR or batch data rate code of the longest interval not above *interval_us, saturating at the
//! shortest, which it writes back; 0 (off) for 0.
static uint8_t prv_odr(uint32_t *interval_us) {
  uint8_t i;

  if (*interval_us == 0U) {
    return 0U;
  }

  for (i = 0U; i < PBL_ARRAY_SIZE(s_odr_us) - 1U; i++) {
    if (*interval_us >= s_odr_us[i]) {
      break;
    }
  }

  *interval_us = s_odr_us[i];

  return i + 1U;
}

//! FIFO words queued while the faster batched sensor queues num_frames
static uint32_t prv_frames_to_words(uint32_t num_frames, uint32_t xl_us, uint32_t gyro_us) {
  if (xl_us == 0U || gyro_us == 0U) {
    return num_frames;
  }

  return num_frames + (num_frames * PBL_MIN(xl_us, gyro_us)) / PBL_MAX(xl_us, gyro_us);
}

//! Largest batch, in frames of the faster batched sensor, that fits the FIFO reads
static uint16_t prv_batch_max(void) {
  const uint32_t xl_us = s_dev->state->xl_req_us != 0U ? s_dev->state->sampling_interval_us : 0U;
  const uint32_t gyro_us = s_dev->state->gyro_interval_us;

  if (xl_us == 0U || gyro_us == 0U) {
    return LSM6DSO_FIFO_SIZE;
  }

  return (LSM6DSO_FIFO_SIZE * PBL_MAX(xl_us, gyro_us)) / (xl_us + gyro_us);
}

static bool prv_lsm6dso_enable_fifo(uint16_t num_samples) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  uint32_t xl_us = state->xl_req_us != 0U ? state->sampling_interval_us : 0U;
  uint32_t gyro_us = state->gyro_interval_us;
  uint8_t bdr_xl = prv_odr(&xl_us);
  uint8_t bdr_gy = prv_odr(&gyro_us);
  bool ret;
  uint8_t val;
  uint16_t wtm;

  // Bypass mode to flush the FIFO
  val = PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL4_MODE_MASK, LSM6DSO_FIFO_CTRL4_MODE_BYPASS);
  ret = prv_lsm6dso_write(LSM6DSO_FIFO_CTRL4, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write FIFO_CTRL4 register");
    return ret;
  }

  // Watermark threshold WTM[8:0]: low 8 bits in FIFO_CTRL1, MSB in FIFO_CTRL2
  wtm = PBL_MIN(prv_frames_to_words(num_samples, xl_us, gyro_us), LSM6DSO_FIFO_SIZE);

  val = PBL_FIELD_GET(PBL_GENMASK(7, 0), wtm);
  ret = prv_lsm6dso_write(LSM6DSO_FIFO_CTRL1, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write FIFO_CTRL1 register");
    return ret;
  }

  val = PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL2_WTM8, PBL_FIELD_GET(PBL_BIT(8), wtm));
  ret = prv_lsm6dso_write(LSM6DSO_FIFO_CTRL2, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write FIFO_CTRL2 register");
    return ret;
  }

  val = PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL3_BDR_XL_MASK, bdr_xl) |
        PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL3_BDR_GY_MASK, bdr_gy);
  ret = prv_lsm6dso_write(LSM6DSO_FIFO_CTRL3, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write FIFO_CTRL3 register");
    return ret;
  }

  // Continuous (stream) mode
  val = PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL4_MODE_MASK, LSM6DSO_FIFO_CTRL4_MODE_STREAM);
  ret = prv_lsm6dso_write(LSM6DSO_FIFO_CTRL4, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write FIFO_CTRL4 register");
    return ret;
  }

  state->fifo_xl_us = xl_us;
  state->fifo_gyro_us = gyro_us;
  state->word_interval_us = (xl_us != 0U && gyro_us != 0U)
                                ? (uint32_t)(((uint64_t)xl_us * gyro_us) / (xl_us + gyro_us))
                                : xl_us + gyro_us;
  state->wtm = wtm;

  PBL_LOG_DBG("FIFO enabled with threshold %" PRIu16, wtm);

  return true;
}

//! Salvage and dispatch any samples queued in the FIFO
static void prv_lsm6dso_drain_fifo(void) {
  for (unsigned int i = 0U; i < LSM6DSO_DRAIN_CHUNKS_MAX; i++) {
    uint8_t status[2];
    uint16_t samples;

    if (!prv_lsm6dso_read(LSM6DSO_FIFO_STATUS1, status, 2)) {
      PBL_LOG_ERR("Could not read FIFO_STATUS registers");
      return;
    }

    samples = (PBL_FIELD_GET(LSM6DSO_FIFO_STATUS2_DIFF_HI_MASK, status[1]) << 8U) | status[0];
    samples = PBL_MIN(samples, LSM6DSO_FIFO_SIZE);
    if (samples == 0U) {
      return;
    }

    uint8_t *buf = prv_alloc_buf();
    if (buf == nullptr) {
      return;
    }

    if (!prv_lsm6dso_read_fifo(buf, samples)) {
      pbl_sensor_buf_free(&s_dev->sensor, buf);
      PBL_LOG_ERR("Failed to read samples");
      return;
    }

    prv_lsm6dso_process_samples(buf, samples, pbl_uptime_us());
    s_dev->state->last_fifo_read_tick = pbl_uptime_ticks();
  }
}

static void prv_lsm6dso_recover(void);
static uint32_t prv_ms_since_last_fifo_read(void);

static void prv_lsm6dso_dispatch_shake(uint8_t wake_up_src) {
  uint8_t raw[LSM6DSO_SAMPLE_SIZE_BYTES] = {0U};
  uint8_t axis = PBL_SENSOR_AXIS_Z;
  int16_t val = 0;
  bool found = false;

  if (!prv_lsm6dso_read(LSM6DSO_OUTX_L_A, raw, sizeof(raw))) {
    PBL_LOG_ERR("Failed to read sample");
  }

  for (uint8_t a = PBL_SENSOR_AXIS_X; a <= PBL_SENSOR_AXIS_Z; a++) {
    int16_t v;

    if ((wake_up_src & LSM6DSO_WAKE_UP_SRC_AXIS_WU(a)) == 0U) {
      continue;
    }

    v = (int16_t)pbl_get_le16(&raw[a * 2U]);
    if (!found || PBL_ABS(v) > PBL_ABS(val)) {
      axis = a;
      val = v;
      found = true;
    }
  }

  pbl_sensor_report_event(&s_dev->sensor, &(const struct pbl_sensor_event){
                                            .type = PBL_SENSOR_EVENT_SHAKE,
                                            .axis = axis,
                                            .direction = (val < 0) ? -1 : 1,
                                          });
}

static void prv_drain_status_done(struct pbl_i2c_async *xfer, int status);
static void prv_drain_data_done(struct pbl_i2c_async *xfer, int status);

//! Drain the FIFO with asynchronous transfers, from any context, unless a drain is in flight or
//! drains are blocked. irq_us is the time of the INT1 edge starting it, or 0.
static void prv_drain_start(uint64_t irq_us) {
  struct pbl_lsm6dso_state *state = s_dev->state;

  pbl_irq_lock();
  if (state->drain_blocked > 0U || state->num_samples == 0U) {
    pbl_irq_unlock();
    return;
  }

  if (state->drain_busy) {
    state->drain_again = true;
    pbl_irq_unlock();
    return;
  }

  state->drain_busy = true;
  state->drain_again = false;
  pbl_irq_unlock();

  state->drain_irq_us = irq_us;
  state->drain_reg = LSM6DSO_FIFO_STATUS1;
  state->drain_msgs[0] = (struct pbl_i2c_msg){
    .buf = &state->drain_reg,
    .len = 1U,
    .flags = PBL_I2C_MSG_WRITE,
  };
  // WTM/OVR flags and DIFF must come from one burst read: a split read can
  // catch the FIFO counter mid-update and see WTM_IA set with DIFF reading 0
  state->drain_msgs[1] = (struct pbl_i2c_msg){
    .buf = state->drain_status,
    .len = sizeof(state->drain_status),
    .flags = PBL_I2C_MSG_READ,
  };
  state->drain_xfer = (struct pbl_i2c_async){
    .dev = &s_dev->i2c,
    .msgs = state->drain_msgs,
    .num_msgs = PBL_ARRAY_SIZE(state->drain_msgs),
    .done = prv_drain_status_done,
  };

  if (pbl_i2c_transfer_async(&state->drain_xfer) != 0) {
    pbl_irq_lock();
    state->drain_busy = false;
    pbl_irq_unlock();
  }
}

//! Words above the watermark keep the level-triggered pad high without a new edge: drain again
//! while that makes progress
static void prv_drain_end(bool progress) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  bool again;

  pbl_irq_lock();
  again = state->drain_again;
  state->drain_busy = false;
  state->drain_again = false;
  pbl_irq_unlock();

  if (progress && (again || pbl_gpio_get(&s_dev->int1_in))) {
    prv_drain_start(0U);
  }
}

static void prv_drain_status_done(struct pbl_i2c_async *xfer, int status) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  uint16_t words;

  if (status != 0) {
    prv_drain_end(false);
    return;
  }

  if ((state->drain_status[1] & LSM6DSO_FIFO_STATUS2_FIFO_OVR_IA) != 0U) {
    (void)pbl_work_submit(&pbl_sys_workq_high, &state->overrun_work);
    prv_drain_end(false);
    return;
  }

  words = (PBL_FIELD_GET(LSM6DSO_FIFO_STATUS2_DIFF_HI_MASK, state->drain_status[1]) << 8U) |
          state->drain_status[0];
  words = PBL_MIN(words, LSM6DSO_FIFO_SIZE);
  if ((state->drain_status[1] & LSM6DSO_FIFO_STATUS2_FIFO_WTM_IA) == 0U || words == 0U) {
    prv_drain_end(false);
    return;
  }

  // Without a free buffer the words stay queued, for the stall watchdog
  state->drain_buf = pbl_sensor_buf_alloc(&s_dev->sensor, nullptr);
  if (state->drain_buf == nullptr) {
    prv_drain_end(false);
    return;
  }

  state->drain_status_us = pbl_uptime_us();
  state->drain_words = words;
  state->drain_reg = LSM6DSO_FIFO_DATA_OUT_TAG;
  state->drain_msgs[1] = (struct pbl_i2c_msg){
    .buf = state->drain_buf,
    .len = words * LSM6DSO_FIFO_WORD_SIZE_BYTES,
    .flags = PBL_I2C_MSG_READ | PBL_I2C_MSG_DMA_SAFE,
  };
  state->drain_xfer.done = prv_drain_data_done;

  if (pbl_i2c_transfer_async(&state->drain_xfer) != 0) {
    pbl_sensor_buf_free(&s_dev->sensor, state->drain_buf);
    prv_drain_end(false);
  }
}

static void prv_drain_data_done(struct pbl_i2c_async *xfer, int status) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  uint64_t t_last_us = state->drain_status_us;

  if (status != 0) {
    pbl_sensor_buf_free(&s_dev->sensor, state->drain_buf);
    prv_drain_end(false);
    return;
  }

  // The edge came as the watermark word was queued
  if (state->drain_irq_us != 0U && state->drain_words >= state->wtm) {
    t_last_us = PBL_MIN(
        state->drain_irq_us + (uint64_t)(state->drain_words - state->wtm) * state->word_interval_us,
        t_last_us);
  }

  prv_lsm6dso_process_samples(state->drain_buf, state->drain_words, t_last_us);
  state->last_fifo_read_tick = pbl_uptime_ticks();

  prv_drain_end(true);
}

static bool prv_drain_idle(void) {
  bool idle;

  pbl_irq_lock();
  idle = !s_dev->state->drain_busy;
  pbl_irq_unlock();

  return idle;
}

//! Stop FIFO drains and wait for the one in flight, around a reconfiguration. Nests.
static void prv_drain_block(void) {
  pbl_irq_lock();
  s_dev->state->drain_blocked++;
  pbl_irq_unlock();

  (void)PBL_POLL_SLEEP_MS(prv_drain_idle(), 1, LSM6DSO_DRAIN_WAIT_MS);
}

//! Allow FIFO drains again, catching up with a watermark reached while blocked
static void prv_drain_unblock(void) {
  bool unblocked;

  pbl_irq_lock();
  unblocked = --s_dev->state->drain_blocked == 0U;
  pbl_irq_unlock();

  if (unblocked && pbl_gpio_get(&s_dev->int1_in)) {
    prv_drain_start(0U);
  }
}

//! INT1 servicing pass kind, logged by the no-action diagnostic
enum int1_pass {
  INT1_PASS_INITIAL = 0,
  INT1_PASS_RETRY = 2,
};

//! Single shake servicing pass; returns true if a wake-up was handled
static bool prv_lsm6dso_service_int1(enum int1_pass pass) {
  uint8_t wake_up_src = 0U;
  bool shake = false;

  if (s_dev->state->shake_detection_enabled) {
    if (!prv_lsm6dso_read(LSM6DSO_WAKE_UP_SRC, &wake_up_src, 1)) {
      PBL_LOG_ERR("Could not read WAKE_UP_SRC register");
      return false;
    }

    shake = (wake_up_src & LSM6DSO_WAKE_UP_SRC_WU_IA) != 0U;
  }

  // WU_IA stays set while the wake-up condition persists (LIR), so only
  // dispatch on its assertion to avoid flooding events
  if (shake && !s_dev->state->wu_active) {
    prv_lsm6dso_dispatch_shake(wake_up_src);
  }
  s_dev->state->wu_active = shake;

  // While streaming, the edge may have been the FIFO's
  if (!shake && s_dev->state->num_samples == 0U) {
    PBL_LOG_WRN("INT1 triggered but no action taken (WAKE_UP_SRC 0x%02" PRIx8
                " shake_en %d pass %u)",
                wake_up_src, s_dev->state->shake_detection_enabled, (unsigned int)pass);
  }

  return shake;
}

static void prv_work_cb(void *data) {
  void (*work)(void) = data;

  pbl_sensor_lock(&s_dev->sensor);
  work();
  pbl_sensor_unlock(&s_dev->sensor);
}

//! Run driver work on the timer task, with the device lock held
static void prv_queue_work(void (*work)(void)) {
  pbl_sys_work_call_high(prv_work_cb, (void *)work);
}

static void prv_queue_work_from_isr(void (*work)(void)) {
  pbl_sys_work_call_high_from_isr(prv_work_cb, (void *)work);
}

static void prv_lsm6dso_int1_work_handler(void) {
  bool action_taken;

  action_taken = prv_lsm6dso_service_int1(INT1_PASS_INITIAL);

  // A source asserting while the pad is high produces no new edge. While
  // streaming, the FIFO drain catches up and a stuck pad is left to the stall
  // watchdog, as is a pad held by a persistent wake-up condition.
  if (!pbl_gpio_get(&s_dev->int1_in)) {
    return;
  }

  if (s_dev->state->num_samples > 0U) {
    prv_drain_start(0U);
    return;
  }

  if (action_taken) {
    return;
  }

  // A source asserting between the status read and the pad re-check is
  // indistinguishable from a stuck pad, and recovering would discard it.
  // Retry the servicing pass once and only recover if the pad is still high
  // with nothing serviced.
  action_taken = prv_lsm6dso_service_int1(INT1_PASS_RETRY);
  if (!action_taken && pbl_gpio_get(&s_dev->int1_in)) {
    prv_lsm6dso_recover();
  }
}

static void prv_lsm6dso_int1_irq_handler(void *arg) {
  const uint64_t now_us = pbl_uptime_us();

  s_dev->state->num_int1_irqs++;
  // A rising edge proves the pad was low, i.e. the wake-up source deasserted
  s_dev->state->wu_active = false;
  // ... which also breaks any stuck-high streak the watchdog counted
  s_dev->state->shake_stuck_passes = 0U;

  prv_drain_start(now_us);

  if (s_dev->state->shake_detection_enabled || s_dev->state->num_samples == 0U) {
    prv_queue_work_from_isr(prv_lsm6dso_int1_work_handler);
  }
}

static void prv_overrun_work_cb(struct pbl_work *work) {
  pbl_sensor_lock(&s_dev->sensor);
  PBL_LOG_WRN("FIFO overrun detected, recovering");
  prv_lsm6dso_recover();
  pbl_sensor_unlock(&s_dev->sensor);
}

//! Apply the accelerometer interval requested, forced to 12.5 Hz for shake detection, and the
//! gyroscope interval
static bool prv_configure_rates(uint32_t xl_req_us, uint32_t gyro_us,
                                bool shake_detection_enabled) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  uint32_t xl_us = (xl_req_us == 0U && shake_detection_enabled) ? 80000UL : xl_req_us;
  uint8_t xl_odr = prv_odr(&xl_us);
  uint8_t gyro_odr = prv_odr(&gyro_us);
  bool ulp = gyro_odr == 0U && xl_odr <= LSM6DSO_ODR_ULP_MAX;
  uint8_t val;

  PBL_LOG_DBG("Configuring intervals to %" PRIu32 " us (accel) %" PRIu32 " us (gyro)", xl_us,
              gyro_us);

  if (ulp != state->ulp) {
    val = prv_fs_bits();
    if (!prv_lsm6dso_write(LSM6DSO_CTRL1_XL, &val, 1)) {
      return false;
    }

    val = ulp ? LSM6DSO_CTRL5_C_XL_ULP_EN : 0U;
    if (!prv_lsm6dso_write(LSM6DSO_CTRL5_C, &val, 1)) {
      return false;
    }

    state->ulp = ulp;
  }

  val = PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_ODR_MASK, xl_odr) | prv_fs_bits();
  if (!prv_lsm6dso_write(LSM6DSO_CTRL1_XL, &val, 1)) {
    return false;
  }

  val = PBL_FIELD_PREP(LSM6DSO_CTRL2_G_ODR_MASK, gyro_odr) | LSM6DSO_GYRO_FS;
  if (!prv_lsm6dso_write(LSM6DSO_CTRL2_G, &val, 1)) {
    return false;
  }

  // Allow the sensors to stabilize after an ODR/power-mode change
  if (xl_odr != 0U || gyro_odr != 0U) {
    pbl_sleep_ms(10);
  }

  state->xl_req_us = xl_req_us;
  state->sampling_interval_us = xl_us;
  state->gyro_interval_us = gyro_us;

  return true;
}

static bool prv_configure_int1(bool shake_detection_enabled, bool fifo_enabled) {
  bool ret;
  uint8_t int1_ctrl;
  uint8_t md1_cfg;
  uint8_t tap_cfg2;

  int1_ctrl = 0U;
  md1_cfg = 0U;

  if (fifo_enabled) {
    int1_ctrl |= LSM6DSO_INT1_CTRL_FIFO_TH | LSM6DSO_INT1_CTRL_FIFO_OVR;
  }

  if (shake_detection_enabled) {
    md1_cfg |= LSM6DSO_MD1_CFG_INT1_WU;
  }

  ret = prv_lsm6dso_write(LSM6DSO_INT1_CTRL, &int1_ctrl, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write INT1_CTRL register");
    return ret;
  }

  PBL_LOG_DBG("INT1_CTRL configured: %02" PRIx8, int1_ctrl);

  // Basic interrupt functions must be enabled for the wake-up event routing
  tap_cfg2 = shake_detection_enabled ? LSM6DSO_TAP_CFG2_INTERRUPTS_ENABLE : 0U;
  ret = prv_lsm6dso_write(LSM6DSO_TAP_CFG2, &tap_cfg2, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write TAP_CFG2 register");
    return ret;
  }

  ret = prv_lsm6dso_write(LSM6DSO_MD1_CFG, &md1_cfg, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write MD1_CFG register");
    return ret;
  }

  PBL_LOG_DBG("MD1_CFG configured: %02" PRIx8, md1_cfg);

  return true;
}

static void prv_lsm6dso_reconfigure(void);

//! Recover a dead INT1/FIFO stream by re-asserting ODR, FIFO and INT routing.
//! Routing is quiesced first so a latched-high pad produces a fresh edge.
static void prv_lsm6dso_recover(void) {
  s_dev->state->num_recoveries++;
  PBL_ANALYTICS_ADD(accel_stream_recovery_count, 1);
  PBL_LOG_WRN("Recovering accel stream (count %" PRIu32 ")", s_dev->state->num_recoveries);

  prv_drain_block();
  prv_lsm6dso_reconfigure();
  prv_drain_unblock();
}

static void prv_lsm6dso_reconfigure(void) {
  uint8_t val;

  if (!prv_configure_int1(false, false)) {
    return;
  }

  // Salvage queued samples
  if (s_dev->state->num_samples > 0U) {
    prv_lsm6dso_drain_fifo();
  }

  // Clear any latched function INT source while routing is quiesced
  if (!prv_lsm6dso_read(LSM6DSO_ALL_INT_SRC, &val, 1)) {
    PBL_LOG_ERR("Could not read ALL_INT_SRC register");
    return;
  }

  if (val != 0U) {
    PBL_LOG_WRN("Discarded latched INT sources during recovery (ALL_INT_SRC 0x%02" PRIx8 ")", val);
  }

  if (!prv_configure_rates(s_dev->state->xl_req_us, s_dev->state->gyro_interval_us,
                           s_dev->state->shake_detection_enabled)) {
    PBL_LOG_ERR("Could not configure ODR");
    return;
  }

  if (s_dev->state->num_samples > 0U) {
    if (!prv_lsm6dso_enable_fifo(s_dev->state->num_samples)) {
      return;
    }
  }

  if (!prv_configure_int1(s_dev->state->shake_detection_enabled, s_dev->state->num_samples > 0U)) {
    return;
  }

  s_dev->state->wu_active = false;
  s_dev->state->last_fifo_read_tick = pbl_uptime_ticks();
}

static uint32_t prv_ms_since_last_fifo_read(void) {
  uint64_t last;

  // Written by the drain, from interrupts
  pbl_irq_lock();
  last = s_dev->state->last_fifo_read_tick;
  pbl_irq_unlock();

  return (uint32_t)(((pbl_uptime_ticks() - last) * 1000) / PBL_TICK_HZ);
}

static uint32_t prv_stall_threshold_ms(void) {
  return PBL_MAX(LSM6DSO_STALL_MARGIN * s_dev->state->int1_period_ms, LSM6DSO_STALL_MIN_MS);
}

//! Arm/disarm the INT1 stall watchdog to match the active INT1 sources.
//! Call after num_samples or shake_detection_enabled changes.
static void prv_update_int1_watchdog(void) {
  pbl_wallclock_work_stop(&s_dev->state->int1_wdt_timer);
  s_dev->state->shake_stuck_passes = 0U;

  if (s_dev->state->num_samples > 0U) {
    s_dev->state->last_fifo_read_tick = pbl_uptime_ticks();
    s_dev->state->int1_period_ms = (s_dev->state->word_interval_us * s_dev->state->wtm) / 1000U;
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_dev->state->int1_wdt_timer, 1,
                             PBL_MAX(PBL_DIV_ROUND_UP(s_dev->state->int1_period_ms, 1000UL), 1U));
  } else if (s_dev->state->shake_detection_enabled) {
    pbl_wallclock_work_start(&pbl_sys_workq_high, &s_dev->state->int1_wdt_timer, 1,
                             LSM6DSO_SHAKE_WDT_PERIOD_S);
  }
}

//! Shared by the INT1 watchdog and the peek staleness trigger;
//! re-validates the stall at execution time.
static void prv_stall_check_work_cb(void) {
  uint32_t ms_since_last_read;
  uint8_t fifo_status[2] = {0U, 0U};

  s_dev->state->recovery_pending = false;

  // Sampling may have stopped between scheduling and execution
  if (s_dev->state->num_samples == 0U) {
    // Shake-only mode: re-run the servicing pass if the pad is stuck high
    // (reading the INT source clears the latch); escalate to a full recovery
    // after consecutive passes that never release it.
    if (s_dev->state->shake_detection_enabled && pbl_gpio_get(&s_dev->int1_in)) {
      prv_lsm6dso_int1_work_handler();
      // A pad released by the pass is healthy; count only a still-high pad
      if (!pbl_gpio_get(&s_dev->int1_in)) {
        s_dev->state->shake_stuck_passes = 0U;
      } else if (++s_dev->state->shake_stuck_passes >= LSM6DSO_SHAKE_STUCK_PASSES_MAX) {
        PBL_LOG_WRN("INT1 pad stuck high for %" PRIu8 " shake watchdog passes, recovering",
                    s_dev->state->shake_stuck_passes);
        prv_lsm6dso_recover();
        s_dev->state->shake_stuck_passes = 0U;
      }
    } else {
      s_dev->state->shake_stuck_passes = 0U;
    }
    return;
  }

  ms_since_last_read = prv_ms_since_last_fifo_read();
  if (ms_since_last_read < prv_stall_threshold_ms()) {
    return;
  }

  // Tells a lost edge (pad high) from an IMU not raising INT1 (pad low)
  if (!prv_lsm6dso_read(LSM6DSO_FIFO_STATUS1, fifo_status, 2)) {
    PBL_LOG_ERR("Could not read FIFO_STATUS registers");
  }

  PBL_LOG_WRN("FIFO stream stalled for %" PRIu32 " ms (INT1 pad %d irqs %" PRIu32
              " FIFO_STATUS1 0x%02" PRIx8 " FIFO_STATUS2 0x%02" PRIx8 ")",
              ms_since_last_read, pbl_gpio_get(&s_dev->int1_in), s_dev->state->num_int1_irqs,
              fifo_status[0], fifo_status[1]);
  prv_lsm6dso_recover();
}

static void prv_int1_wdt_cb(struct pbl_work *work) {
  prv_queue_work(prv_stall_check_work_cb);
}

////////////////////////////////////////////////////////////////////////////////
// Sensor interface
////////////////////////////////////////////////////////////////////////////////

static int prv_init(const struct pbl_sensor *sensor) {
  bool ret;
  int err;
  uint8_t val;

  s_dev = PBL_CONTAINER_OF(sensor, const struct pbl_lsm6dso, sensor);

  // Check device ID
  ret = prv_lsm6dso_read(LSM6DSO_WHO_AM_I, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not read WHO_AM_I register");
    return -ENODEV;
  }

  if (val != LSM6DSO_WHO_AM_I_VAL) {
    PBL_LOG_ERR("Unexpected id: 0x%02X!=0x%02X", val, LSM6DSO_WHO_AM_I_VAL);
    return -ENODEV;
  }

  // Perform a software reset (so we can rely on defaults)
  val = LSM6DSO_CTRL3_C_SW_RESET;
  ret = prv_lsm6dso_write(LSM6DSO_CTRL3_C, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL3_C register");
    return -EIO;
  }

  pbl_busy_wait_us(LSM6DSO_RESET_TIME_US);

  do {
    ret = prv_lsm6dso_read(LSM6DSO_CTRL3_C, &val, 1);
    if (!ret) {
      PBL_LOG_ERR("Could not read CTRL3_C register");
      return -EIO;
    }
  } while ((val & LSM6DSO_CTRL3_C_SW_RESET) != 0U);

  // Enable register auto-increment and block data update
  val = LSM6DSO_CTRL3_C_IF_INC | LSM6DSO_CTRL3_C_BDU;
  ret = prv_lsm6dso_write(LSM6DSO_CTRL3_C, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL3_C register");
    return -EIO;
  }

  // Disable I3C interface
  val = LSM6DSO_CTRL9_XL_I3C_DISABLE;
  ret = prv_lsm6dso_write(LSM6DSO_CTRL9_XL, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL9_XL register");
    return -EIO;
  }

  // Ultra-low-power mode (accelerometer still powered down here, as required to
  // toggle XL_ULP_EN); the gyroscope is powered down after the reset
  val = LSM6DSO_CTRL5_C_XL_ULP_EN;
  ret = prv_lsm6dso_write(LSM6DSO_CTRL5_C, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL5_C register");
    return -EIO;
  }

  s_dev->state->ulp = true;

  // Configure scale (ODR off until sampling is requested)
  val = PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_ODR_MASK, LSM6DSO_CTRL1_XL_ODR_OFF) | prv_fs_bits();
  ret = prv_lsm6dso_write(LSM6DSO_CTRL1_XL, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL1_XL register");
    return -EIO;
  }

  // Slope filter for wake-up, latch interrupts and clear them on read
  val = LSM6DSO_TAP_CFG0_LIR | LSM6DSO_TAP_CFG0_INT_CLR_ON_READ;
  ret = prv_lsm6dso_write(LSM6DSO_TAP_CFG0, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write TAP_CFG0 register");
    return -EIO;
  }

  // Configure wake-up threshold defaults
  val = PBL_FIELD_PREP(LSM6DSO_WAKE_UP_DUR_WAKE_DUR_MASK, CONFIG_ACCEL_LSM6DSO_WK_DUR_DEFAULT);
  ret = prv_lsm6dso_write(LSM6DSO_WAKE_UP_DUR, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write WAKE_UP_DUR register");
    return -EIO;
  }

  val = PBL_FIELD_PREP(LSM6DSO_WAKE_UP_THS_WK_THS_MASK, CONFIG_ACCEL_LSM6DSO_WK_THS_DEFAULT);
  ret = prv_lsm6dso_write(LSM6DSO_WAKE_UP_THS, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write WAKE_UP_THS register");
    return -EIO;
  }

  s_dev->state->wk_ths_curr = CONFIG_ACCEL_LSM6DSO_WK_THS_DEFAULT;

  err = pbl_device_init(&s_dev->int1.intc->dev);
  if (err != 0) {
    PBL_LOG_ERR("INT1 interrupt controller unavailable");
    return err;
  }

  err = pbl_gpio_configure(&s_dev->int1_in, PBL_GPIO_INPUT);
  if (err != 0) {
    PBL_LOG_ERR("Could not configure the INT1 input (%d)", err);
    return err;
  }

  err = pbl_intc_request(&s_dev->int1, prv_lsm6dso_int1_irq_handler, nullptr);
  if (err != 0) {
    PBL_LOG_ERR("Could not request INT1 (%d)", err);
    return err;
  }

  err = pbl_intc_enable(&s_dev->int1);
  if (err != 0) {
    PBL_LOG_ERR("Could not enable INT1 (%d)", err);
    return err;
  }

  pbl_wallclock_work_init(&s_dev->state->int1_wdt_timer, prv_int1_wdt_cb);
  pbl_work_init(&s_dev->state->overrun_work, prv_overrun_work_cb);

  return 0;
}

static int prv_set_sampling_interval(enum pbl_sensor_type type, uint32_t interval_us,
                                     uint32_t *actual_us) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  const uint32_t prev_interval_us = state->sampling_interval_us;
  uint32_t xl_req_us = state->xl_req_us;
  uint32_t gyro_us = state->gyro_interval_us;
  const bool streaming = state->num_samples > 0U;

  if (type == PBL_SENSOR_GYRO) {
    gyro_us = interval_us;
  } else {
    xl_req_us = interval_us;
  }

  prv_drain_block();

  // Report the queued frames with the intervals they were sampled at
  if (streaming) {
    (void)prv_configure_int1(false, false);
    state->recovery_pending = false;
    prv_lsm6dso_drain_fifo();
  }

  if (!prv_configure_rates(xl_req_us, gyro_us, state->shake_detection_enabled)) {
    PBL_LOG_ERR("Could not configure ODR");
    prv_drain_unblock();
    return -EIO;
  }

  if (streaming) {
    if (!prv_lsm6dso_enable_fifo(state->num_samples) ||
        !prv_configure_int1(state->shake_detection_enabled, true)) {
      PBL_LOG_ERR("Could not enable FIFO");
      prv_drain_unblock();
      return -EIO;
    }
  }

  prv_drain_unblock();

  // Re-arm the watchdog only on an actual ODR change (a re-arm resets the
  // stall tracking, so repeat calls must not defer it)
  if (streaming || state->sampling_interval_us != prev_interval_us) {
    prv_update_int1_watchdog();
  }

  *actual_us = (type == PBL_SENSOR_GYRO) ? state->gyro_interval_us : state->sampling_interval_us;

  return 0;
}

static int prv_apply_num_samples(uint32_t num_samples) {
  bool ret;
  uint8_t val;

  num_samples = PBL_MIN(num_samples, prv_batch_max());

  // Disable all INT1 before changing FIFO threshold
  prv_configure_int1(false, false);

  // Config change invalidates any queued stall check; re-arm the peek trigger
  s_dev->state->recovery_pending = false;

  // Salvage queued samples
  if (s_dev->state->num_samples > 0U) {
    prv_lsm6dso_drain_fifo();
  }

  if (num_samples == 0U) {
    // Bypass FIFO (disable)
    val = PBL_FIELD_PREP(LSM6DSO_FIFO_CTRL4_MODE_MASK, LSM6DSO_FIFO_CTRL4_MODE_BYPASS);
    if (!prv_lsm6dso_write(LSM6DSO_FIFO_CTRL4, &val, 1)) {
      PBL_LOG_ERR("Could not write FIFO_CTRL4 register");
    }
  } else {
    // Configure FIFO in continuous mode with threshold
    ret = prv_lsm6dso_enable_fifo((uint16_t)num_samples);
    if (!ret) {
      PBL_LOG_ERR("Could not enable FIFO");
      return -EIO;
    }
  }

  // Re-configure INT1
  ret = prv_configure_int1(s_dev->state->shake_detection_enabled, num_samples > 0U);
  if (!ret) {
    PBL_LOG_ERR("Could not configure INT1");
    return -EIO;
  }

  s_dev->state->num_samples = num_samples;
  // Arm/disarm the INT1 liveness watchdog for the new source configuration
  prv_update_int1_watchdog();

  PBL_LOG_DBG("Set number of samples to %" PRIu32, num_samples);

  return 0;
}

static int prv_set_num_samples(uint32_t num_samples) {
  int ret;

  prv_drain_block();
  ret = prv_apply_num_samples(num_samples);
  prv_drain_unblock();

  return ret;
}

//! True once a sample is ready, or when the status read fails.
static bool prv_data_ready(bool *read_ok) {
  uint8_t status;
  *read_ok = prv_lsm6dso_read(LSM6DSO_STATUS_REG, &status, 1);
  return !*read_ok || (status & LSM6DSO_STATUS_REG_XLDA) != 0U;
}

static bool prv_gyro_ready(bool *read_ok) {
  uint8_t status;
  *read_ok = prv_lsm6dso_read(LSM6DSO_STATUS_REG, &status, 1);
  return !*read_ok || (status & LSM6DSO_STATUS_REG_GDA) != 0U;
}

static int prv_read_gyro(struct pbl_sensor_frame *frame) {
  struct pbl_lsm6dso_state *state = s_dev->state;
  bool read_ok;
  int err = 0;
  int rc;

  frame->chans = s_gyro_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_gyro_chans);

  if (state->gyro_interval_us != 0U) {
    if (!prv_lsm6dso_read(LSM6DSO_OUTX_L_G, frame->data, LSM6DSO_SAMPLE_SIZE_BYTES)) {
      return -EIO;
    }

    frame->timestamp_us = pbl_uptime_us();

    return 0;
  }

  // Run the gyroscope at 104 Hz, unbatched, for one sample past its turn-on time
  if (!prv_configure_rates(state->xl_req_us, 9615U, state->shake_detection_enabled)) {
    return -EIO;
  }

  pbl_sleep_ms(LSM6DSO_GYRO_TURN_ON_MS);

  rc = PBL_POLL_SLEEP_MS(prv_gyro_ready(&read_ok), LSM6DSO_DRDY_POLL_DELAY_MS,
                         LSM6DSO_DRDY_POLL_TIMEOUT_MS);
  if (!read_ok || rc < 0 ||
      !prv_lsm6dso_read(LSM6DSO_OUTX_L_G, frame->data, LSM6DSO_SAMPLE_SIZE_BYTES)) {
    err = -EIO;
  }

  frame->timestamp_us = pbl_uptime_us();

  if (!prv_configure_rates(state->xl_req_us, 0U, state->shake_detection_enabled)) {
    err = -EIO;
  }

  return err;
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  int err = 0;
  bool ret;
  uint8_t ctrl1;
  uint8_t ctrl1_bck;
  uint8_t *raw = frame->data;

  if (type == PBL_SENSOR_GYRO) {
    return prv_read_gyro(frame);
  }

  frame->chans = s_xl_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_xl_chans);

  // A running accelerometer updates its output registers at ODR, independently
  // of the FIFO, which may hold samples a full watermark period old
  if (s_dev->state->sampling_interval_us != 0U) {
    // Self-heal: a stalled stream would otherwise freeze peek data forever
    if (s_dev->state->num_samples > 0U && !s_dev->state->recovery_pending &&
        (prv_ms_since_last_fifo_read() >= prv_stall_threshold_ms())) {
      s_dev->state->recovery_pending = true;
      prv_queue_work(prv_stall_check_work_cb);
    }

    ret = prv_lsm6dso_read(LSM6DSO_OUTX_L_A, raw, LSM6DSO_SAMPLE_SIZE_BYTES);
    if (!ret) {
      PBL_LOG_ERR("Failed to read sample");
      return -EIO;
    }

    frame->timestamp_us = pbl_uptime_us();

    return 0;
  }

  // Save CTRL1_XL
  ret = prv_lsm6dso_read(LSM6DSO_CTRL1_XL, &ctrl1_bck, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not read CTRL1_XL register");
    return -EIO;
  }

  // Enable continuous conversion at 52Hz to obtain a single measurement
  ctrl1 = PBL_FIELD_PREP(LSM6DSO_CTRL1_XL_ODR_MASK, LSM6DSO_CTRL1_XL_ODR_52HZ) | prv_fs_bits();
  ret = prv_lsm6dso_write(LSM6DSO_CTRL1_XL, &ctrl1, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write CTRL1_XL register");
    return -EIO;
  }

  // Poll for data ready (timeout after 100ms, ~5x the expected 20ms at 52Hz ODR)
  bool read_ok;
  int rc = PBL_POLL_SLEEP_MS(prv_data_ready(&read_ok), LSM6DSO_DRDY_POLL_DELAY_MS,
                             LSM6DSO_DRDY_POLL_TIMEOUT_MS);
  if (!read_ok) {
    PBL_LOG_ERR("Could not read STATUS_REG register");
    err = -EIO;
    goto end;
  }
  if (rc < 0) {
    PBL_LOG_ERR("DRDY timeout after %d ms", LSM6DSO_DRDY_POLL_TIMEOUT_MS);
    err = -EIO;
    goto end;
  }

  // Read sample
  ret = prv_lsm6dso_read(LSM6DSO_OUTX_L_A, raw, LSM6DSO_SAMPLE_SIZE_BYTES);
  if (!ret) {
    PBL_LOG_ERR("Failed to read sample");
    err = -EIO;
    goto end;
  }

  frame->timestamp_us = pbl_uptime_us();

end:
  // Restore CTRL1_XL (back to previous state, e.g. power-down or shake ODR)
  (void)prv_lsm6dso_write(LSM6DSO_CTRL1_XL, &ctrl1_bck, 1);

  return err;
}

static int prv_set_shake_detection(bool on) {
  bool ret;

  ret = prv_configure_rates(s_dev->state->xl_req_us, s_dev->state->gyro_interval_us, on);
  if (!ret) {
    PBL_LOG_ERR("Could not configure ODR");
    return -EIO;
  }

  if (on) {
    uint8_t val;

    // WU_IA may be latched from a previous enablement or an ODR startup
    // transient; clear it so a phantom shake is not dispatched on enable
    if (!prv_lsm6dso_read(LSM6DSO_ALL_INT_SRC, &val, 1)) {
      PBL_LOG_ERR("Could not read ALL_INT_SRC register");
    }
  }

  // Configure INT1
  ret = prv_configure_int1(on, s_dev->state->num_samples > 0U);
  if (!ret) {
    PBL_LOG_ERR("Could not configure INT1");
    return -EIO;
  }

  s_dev->state->shake_detection_enabled = on;
  s_dev->state->wu_active = false;
  // Arm/disarm the INT1 liveness watchdog now that shake routing changed
  prv_update_int1_watchdog();

  PBL_LOG_DBG("%s shake detection", on ? "Enabled" : "Disabled");

  return 0;
}

static int prv_set_shake_sensitivity_percent(uint8_t percent) {
  bool ret;
  uint8_t val;
  uint8_t raw;

  // Reverse mapping: 0 = min sensitivity (MAX threshold), 100 = max sensitivity (MIN threshold)
  // [0, 100] -> [wk_ths_max, wk_ths_min]
  raw = CONFIG_ACCEL_LSM6DSO_WK_THS_MAX -
        (percent * (CONFIG_ACCEL_LSM6DSO_WK_THS_MAX - CONFIG_ACCEL_LSM6DSO_WK_THS_MIN)) / 100U;

  val = PBL_FIELD_PREP(LSM6DSO_WAKE_UP_THS_WK_THS_MASK, raw);
  ret = prv_lsm6dso_write(LSM6DSO_WAKE_UP_THS, &val, 1);
  if (!ret) {
    PBL_LOG_ERR("Could not write WAKE_UP_THS register");
    return -EIO;
  }

  s_dev->state->wk_ths_curr = raw;

  PBL_LOG_DBG("Configured shake sensitivity to %" PRIu8 " (%" PRIu8 ")", percent, raw);

  return 0;
}

static int prv_attr_set(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  uint32_t actual_us;
  int ret;

  switch (attr) {
    case PBL_SENSOR_ATTR_INTERVAL:
      if (*val < 0) {
        return -EINVAL;
      }
      ret = prv_set_sampling_interval(type, (uint32_t)*val, &actual_us);
      if (ret == 0) {
        *val = (int32_t)actual_us;
      }
      return ret;
    case PBL_SENSOR_ATTR_BATCH:
      *val = PBL_CLAMP(*val, 0, (int32_t)prv_batch_max());
      return prv_set_num_samples((uint32_t)*val);
    case PBL_SENSOR_ATTR_SHAKE_SENSITIVITY:
      if (type != PBL_SENSOR_ACCEL) {
        return -ENOTSUP;
      }
      *val = PBL_CLAMP(*val, 0, 100);
      return prv_set_shake_sensitivity_percent((uint8_t)*val);
    default:
      return -ENOTSUP;
  }
}

static int prv_attr_get(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                        enum pbl_sensor_attr attr, int32_t *val) {
  switch (attr) {
    case PBL_SENSOR_ATTR_INTERVAL:
      *val = (int32_t)(type == PBL_SENSOR_GYRO ? s_dev->state->gyro_interval_us
                                               : s_dev->state->sampling_interval_us);
      return 0;
    case PBL_SENSOR_ATTR_BATCH:
      *val = s_dev->state->num_samples;
      return 0;
    case PBL_SENSOR_ATTR_BATCH_MAX:
      *val = prv_batch_max();
      return 0;
    default:
      return -ENOTSUP;
  }
}

static int prv_set_event(const struct pbl_sensor *sensor, enum pbl_sensor_event_type type,
                         bool on) {
  if (type != PBL_SENSOR_EVENT_SHAKE) {
    return -ENOTSUP;
  }

  return prv_set_shake_detection(on);
}

const struct pbl_sensor_ops pbl_lsm6dso_ops = {
  .init = prv_init,
  .read = prv_read,
  .attr_set = prv_attr_set,
  .attr_get = prv_attr_get,
  .set_event = prv_set_event,
};

#include PBL_DT_INSTANCES(st_lsm6dso)
