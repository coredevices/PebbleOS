/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pbl/logging/logging.h>
#include <pbl/services/hrm/hrm_ppg.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <goodix_hba.h>
#include <goodix_hrv.h>
#include <goodix_log.h>
#include <goodix_mem.h>
#include <goodix_nadt.h>
#include <goodix_spo2.h>

PBL_LOG_MODULE_DECLARE(service_hrm, CONFIG_SERVICE_HRM_LOG_LEVEL);

/* Memory needed by HR, SpO2, HRV and wear detection together. */
#define POOL_BYTES 30016U

#define ALGO_CHANNELS    4U
#define ALGO_SLOTS       (3U * ALGO_CHANNELS)
#define ALGO_GREEN       0U
#define ALGO_IR          ALGO_CHANNELS
#define ALGO_RED         (2U * ALGO_CHANNELS)
#define ALGO_BITS        24U
#define ALGO_CHIP_GH3X2X 1U
#define ALGO_DATA_ADC    0U
#define ALGO_RATE_HZ     25U
#define VERSION_LEN      20U

#define RAW_ZERO        0x800000
#define RAW_MASK        0xFFFFFFU
#define DRV_CODE_MAX    255U
#define ACCEL_LSB_PER_G 512
#define MG_PER_G        1000
#define SPO2_SCALE      10000
#define NADT_STATE_MASK 0x3
#define NADT_WORN       1
#define NADT_OFF        0x2

/* Receiver gains of the GH3x2x family, in ohms, indexed by gain code. */
static const uint32_t s_gain_codes_ohm[] = {
  10000U,  25000U,   50000U,   75000U,   100000U,  250000U,  500000U,
  750000U, 1000000U, 1250000U, 1500000U, 1750000U, 2000000U,
};

/* Marker the algorithms expect in bit 0 of every sample. */
static const uint32_t s_raw_marks[] = {0x6b851eb7U, 0x4147ae13U, 0x28f5c28fU, 0x15c28f5bU};

static const uint8_t s_scenes[] = {
  [HRMActivityScene_Default] = HBA_SCENES_DEFAULT,
  [HRMActivityScene_Walk] = HBA_SCENES_WALKING_OUTSIDE,
  [HRMActivityScene_Run] = HBA_SCENES_RUNNING_HIGH_HR,
  [HRMActivityScene_HighIntensity] = HBA_SCENES_HIGH_INTENSITY_COMBINE,
};

struct algo_input {
  int32_t raw[ALGO_SLOTS];
  uint8_t gain[ALGO_SLOTS];
  uint16_t drv[ALGO_SLOTS];
  uint8_t enable[(ALGO_SLOTS + 7U) / 8U];
};

static uint32_t s_pool[POOL_BYTES / sizeof(uint32_t)];

static struct {
  const struct hrm_ppg_session *session;
  bool hr;
  bool spo2;
  bool hrv;
  bool nadt;
  uint8_t decimation;
  uint32_t frame;
  uint32_t hr_frame_id;
  uint32_t spo2_frame_id;
  uint32_t hrv_frame_id;
  uint32_t nadt_frame_id;
  uint8_t last_bpm;
  int8_t worn;
  volatile uint8_t scene;
  int8_t green[ALGO_CHANNELS];
  uint8_t num_green;
  int8_t red;
  int8_t ir;
  int8_t wear;
  int8_t ambient;
} s_goodix;

void Gh3x2xPoolIsNotEnough(void) {
  PBL_LOG_ERR("Algorithm memory pool exhausted");
}

static void prv_log(char *str) {
  PBL_LOG_DBG("%s", str);
}

static int32_t prv_snprintf(char *str, size_t size, const char *format, ...) {
  va_list args;
  int ret;

  va_start(args, format);
  ret = vsnprintf(str, size, format, args);
  va_end(args);

  return ret;
}

static int32_t prv_raw(int32_t value) {
  const struct pbl_ppg_info *info = s_goodix.session->info;
  uint32_t raw = (uint32_t)(((int64_t)value * RAW_ZERO) / info->full_scale + RAW_ZERO) & RAW_MASK;
  uint32_t data = (raw >> 1) & (RAW_MASK >> 1);
  uint32_t mark = (uint32_t)__builtin_popcount(data ^ s_raw_marks[(raw >> 1) & 0x3U]) & 1U;

  return (int32_t)((raw & ~1U) | mark);
}

static uint8_t prv_gain_code(uint32_t ohm) {
  uint8_t code = 0U;

  while (((code + 1U) < ARRAY_LENGTH(s_gain_codes_ohm)) && (s_gain_codes_ohm[code + 1U] <= ohm)) {
    code++;
  }

  return code;
}

static uint16_t prv_drv_code(uint32_t ua) {
  uint32_t fs_ua = s_goodix.session->info->max_led_current_ua;

  return (uint16_t)MIN(((uint64_t)ua * DRV_CODE_MAX) / fs_ua, DRV_CODE_MAX);
}

static void prv_put(struct algo_input *in, const struct hrm_ppg_frame *frame, uint8_t pos,
                    int8_t channel, bool settings, bool enable) {
  if (channel < 0) {
    return;
  }

  in->raw[pos] = prv_raw(frame->value[channel]);

  if (settings) {
    in->gain[pos] = prv_gain_code(frame->gain_ohm[channel]);
    in->drv[pos] = prv_drv_code(frame->led_ua[channel]);
  }

  if (enable) {
    in->enable[pos / 8U] |= (uint8_t)(1U << (7U - (pos % 8U)));
  }
}

static void prv_accel(const struct hrm_ppg_frame *frame, int32_t *x, int32_t *y, int32_t *z) {
  *x = (frame->accel_mg[0] * ACCEL_LSB_PER_G) / MG_PER_G;
  *y = (frame->accel_mg[1] * ACCEL_LSB_PER_G) / MG_PER_G;
  *z = (frame->accel_mg[2] * ACCEL_LSB_PER_G) / MG_PER_G;
}

static int prv_init_hr(void) {
  goodix_hba_config config;
  char version[HBA_INTERFACE_VERSION_LEN_MAX] = {};

  memcpy(&config, goodix_hba_config_get_arr(), sizeof(config));
  config.fs = ALGO_RATE_HZ;
  config.valid_ch_num = s_goodix.num_green;
  goodix_hba_config_get_version(version, sizeof(version));

  return (goodix_hba_init(&config, sizeof(config), version) == GX_ALGO_HBA_SUCCESS) ? 0 : -EIO;
}

static int prv_init_nadt(void) {
  goodix_nadt_config config;
  char version[NADT_INTERFACE_VERSION_LEN_MAX] = {};

  memcpy(&config, goodix_nadt_config_get_arr(), sizeof(config));
  config.sampleRateNADT = ALGO_RATE_HZ;
  goodix_nadt_config_get_version(version, sizeof(version));

  return (goodix_nadt_init(&config, sizeof(config), version) == GX_ALGO_NADT_SUCCESS) ? 0 : -EIO;
}

static int prv_init_spo2(void) {
  goodix_spo2_config config;
  char version[SPO2_INTERFACE_VERSION_LEN_MAX] = {};

  memcpy(&config, goodix_spo2_config_get_instance(), sizeof(config));
  config.raw_fs = ALGO_RATE_HZ;
  config.valid_chl_num = 1U;
  goodix_spo2_config_get_version(version, sizeof(version));

  return (goodix_spo2_init(&config, sizeof(config), version) == GX_ALGO_SPO2_SUCCESS) ? 0 : -EIO;
}

static int prv_init_hrv(void) {
  goodix_hrv_config config;
  char version[HRV_INTERFACE_VERSION_LEN_MAX] = {};

  memcpy(&config, goodix_hrv_config_get_arr(), sizeof(config));
  config.fs = s_goodix.session->sample_rate_hz;
  goodix_hrv_config_get_version(version, sizeof(version));

  return (goodix_hrv_init(&config, sizeof(config), version) == GX_HRV_ALGO_OK) ? 0 : -EIO;
}

static void prv_deinit(void) {
  if (s_goodix.hr) {
    (void)goodix_hba_deinit();
  }

  if (s_goodix.nadt) {
    (void)goodix_nadt_deinit();
  }

  if (s_goodix.spo2) {
    (void)goodix_spo2_deinit();
  }

  if (s_goodix.hrv) {
    (void)goodix_hrv_deinit();
  }

  goodix_mem_deinit();
}

static void prv_map_channels(const struct hrm_ppg_session *session) {
  s_goodix.num_green = 0U;
  s_goodix.red = -1;
  s_goodix.ir = -1;
  s_goodix.wear = -1;
  s_goodix.ambient = -1;

  for (uint8_t i = 0U; i < session->num_channels; i++) {
    switch (session->channels[i].role) {
      case HRM_PPG_ROLE_GREEN:
        if (s_goodix.num_green < ALGO_CHANNELS) {
          s_goodix.green[s_goodix.num_green++] = (int8_t)i;
        }
        break;
      case HRM_PPG_ROLE_RED:
        s_goodix.red = (s_goodix.red < 0) ? (int8_t)i : s_goodix.red;
        break;
      case HRM_PPG_ROLE_IR:
        s_goodix.ir = (s_goodix.ir < 0) ? (int8_t)i : s_goodix.ir;
        break;
      case HRM_PPG_ROLE_WEAR:
        s_goodix.wear = (s_goodix.wear < 0) ? (int8_t)i : s_goodix.wear;
        break;
      case HRM_PPG_ROLE_AMBIENT:
        s_goodix.ambient = (s_goodix.ambient < 0) ? (int8_t)i : s_goodix.ambient;
        break;
      default:
        break;
    }
  }
}

static int prv_start_algos(const struct hrm_ppg_session *session) {
  int ret;

  if (s_goodix.num_green > 0U) {
    ret = prv_init_hr();
    if (ret != 0) {
      return ret;
    }
    s_goodix.hr = true;

    if ((s_goodix.wear >= 0) && (s_goodix.ambient >= 0)) {
      ret = prv_init_nadt();
      if (ret != 0) {
        return ret;
      }
      s_goodix.nadt = true;
    }
  }

  if ((session->features & HRMFeature_SpO2) && (s_goodix.red >= 0) && (s_goodix.ir >= 0)) {
    ret = prv_init_spo2();
    if (ret != 0) {
      return ret;
    }
    s_goodix.spo2 = true;
  }

  if ((session->features & HRMFeature_HRV) && (s_goodix.num_green > 0U)) {
    ret = prv_init_hrv();
    if (ret != 0) {
      return ret;
    }
    s_goodix.hrv = true;
  }

  return 0;
}

static int prv_start(const struct hrm_ppg_session *session) {
  uint8_t scene = s_goodix.scene;
  int ret;

  memset(&s_goodix, 0, sizeof(s_goodix));
  s_goodix.session = session;
  s_goodix.scene = scene;
  s_goodix.worn = -1;
  s_goodix.decimation = (uint8_t)MAX(session->sample_rate_hz / ALGO_RATE_HZ, 1U);
  prv_map_channels(session);

  goodix_set_sprintFunc(nullptr, prv_snprintf, prv_log);

  if (goodix_mem_init(s_pool, sizeof(s_pool)) != MEMPOOL_IS_OK_E) {
    return -ENOMEM;
  }

  ret = prv_start_algos(session);
  if (ret != 0) {
    PBL_LOG_ERR("Algorithm init failed (%d)", ret);
    prv_deinit();
    s_goodix.session = nullptr;
  }

  return ret;
}

static void prv_stop(void) {
  if (s_goodix.session == nullptr) {
    return;
  }

  prv_deinit();
  s_goodix.session = nullptr;
}

static void prv_process_hr(const struct hrm_ppg_frame *frame) {
  struct algo_input in = {};
  goodix_hba_input_unique unique = {.scence = s_goodix.scene};
  goodix_hba_input_rawdata raw = {
    .frame_id = s_goodix.hr_frame_id++ & UINT8_MAX,
    .total_ch_num = ALGO_CHANNELS,
    .enable_flg = in.enable,
    .ppg_rawdata = in.raw,
    .ch_agc_gain = in.gain,
    .ch_agc_drv = in.drv,
    .bit_num = ALGO_BITS,
    .chip_type = ALGO_CHIP_GH3X2X,
    .data_type = ALGO_DATA_ADC,
    .pst_module_unique = &unique,
  };
  goodix_hba_result result = {};

  for (uint8_t i = 0U; i < s_goodix.num_green; i++) {
    prv_put(&in, frame, ALGO_GREEN + i, s_goodix.green[i], false, true);
  }

  prv_accel(frame, &raw.acc_x, &raw.acc_y, &raw.acc_z);

  if ((goodix_hba_calc(&raw, &result) == GX_ALGO_HBA_SUCCESS) && (result.hba_out_flag == 1U)) {
    s_goodix.last_bpm = (uint8_t)MIN(result.hba_out, UINT8_MAX);
    hrm_ppg_report_bpm(s_goodix.last_bpm, (uint8_t)MIN(result.valid_score, UINT8_MAX));
  }
}

static void prv_process_nadt(const struct hrm_ppg_frame *frame) {
  struct algo_input in = {};
  int32_t ipd[ALGO_SLOTS] = {};
  goodix_nadt_input_unique unique = {.ch_ipd = ipd};
  goodix_nadt_input_rawdata raw = {
    .frame_id = s_goodix.nadt_frame_id++ & UINT8_MAX,
    .total_ch_num = ALGO_CHANNELS,
    .enable_flg = in.enable,
    .ppg_rawdata = in.raw,
    .ch_agc_gain = in.gain,
    .ch_agc_drv = in.drv,
    .bit_num = ALGO_BITS,
    .chip_type = ALGO_CHIP_GH3X2X,
    .data_type = ALGO_DATA_ADC,
    .pst_module_unique = &unique,
  };
  goodix_nadt_result result = {};
  int8_t worn;

  prv_put(&in, frame, ALGO_GREEN, s_goodix.green[0], true, false);
  prv_put(&in, frame, ALGO_IR, s_goodix.wear, true, false);
  prv_put(&in, frame, ALGO_RED, s_goodix.ambient, true, false);
  prv_accel(frame, &raw.acc_x, &raw.acc_y, &raw.acc_z);

  if ((goodix_nadt_calc(&raw, &result) != GX_ALGO_NADT_SUCCESS) || (result.nadt_out_flag != 1)) {
    return;
  }

  if ((result.nadt_out & NADT_STATE_MASK) == NADT_WORN) {
    worn = 1;
  } else if (result.nadt_out & NADT_OFF) {
    worn = 0;
  } else {
    return;
  }

  if (worn != s_goodix.worn) {
    s_goodix.worn = worn;
    hrm_ppg_report_wear(worn == 1);
  }
}

static void prv_process_spo2(const struct hrm_ppg_frame *frame) {
  struct algo_input in = {};
  goodix_spo2_input_rawdata raw = {
    .frame_id = s_goodix.spo2_frame_id++ & UINT8_MAX,
    .total_ch_num = ALGO_CHANNELS,
    .enable_flg = in.enable,
    .ppg_rawdata = in.raw,
    .ch_agc_gain = in.gain,
    .ch_agc_drv = in.drv,
    .bit_num = ALGO_BITS,
    .chip_type = ALGO_CHIP_GH3X2X,
    .data_type = ALGO_DATA_ADC,
  };
  goodix_spo2_result result = {};
  goodix_spo2_ret ret;

  prv_put(&in, frame, ALGO_IR, s_goodix.ir, true, true);
  prv_put(&in, frame, ALGO_RED, s_goodix.red, true, true);
  prv_accel(frame, &raw.acc_x, &raw.acc_y, &raw.acc_z);

  ret = goodix_spo2_calc(&raw, &result);
  if (((ret != GX_ALGO_SPO2_SUCCESS) && (ret != GX_ALGO_SPO2_FRAME_UNCOMPLETE) &&
       (ret != GX_ALGO_SPO2_WIN_UNCOMPLETE)) ||
      (result.final_calc_flg != 1)) {
    return;
  }

  hrm_ppg_report_spo2((uint8_t)((result.final_spo2 + SPO2_SCALE / 2) / SPO2_SCALE),
                      (uint8_t)CLIP(result.final_confi_coeff, 0, UINT8_MAX),
                      (uint8_t)CLIP(result.final_valid_level, 0, UINT8_MAX),
                      result.final_invalidFlg != 0);
}

static void prv_process_hrv(const struct hrm_ppg_frame *frame) {
  struct algo_input in = {};
  goodix_hrv_input_unique unique = {.hr = s_goodix.last_bpm};
  goodix_hrv_input_rawdata raw = {
    .frame_id = s_goodix.hrv_frame_id++ & UINT8_MAX,
    .total_ch_num = ALGO_CHANNELS,
    .enable_flg = in.enable,
    .ppg_rawdata = in.raw,
    .ch_agc_gain = in.gain,
    .ch_agc_drv = in.drv,
    .bit_num = ALGO_BITS,
    .chip_type = ALGO_CHIP_GH3X2X,
    .data_type = ALGO_DATA_ADC,
    .pst_module_unique = &unique,
  };
  goodix_hrv_result result = {};

  for (uint8_t i = 0U; i < s_goodix.num_green; i++) {
    prv_put(&in, frame, ALGO_GREEN + i, s_goodix.green[i], true, true);
  }

  prv_accel(frame, &raw.acc_x, &raw.acc_y, &raw.acc_z);

  if (goodix_hrv_calc(&raw, &result) != GX_HRV_ALGO_UPDATE) {
    return;
  }

  hrm_ppg_report_hrv(result.rri, (uint8_t)CLIP(result.rri_valid_num, 0, RRI_NUM),
                     (uint8_t)CLIP(result.rri_confidence, 0, UINT8_MAX));
}

static void prv_process(const struct hrm_ppg_frame *frame) {
  bool algo_frame;

  if (s_goodix.session == nullptr) {
    return;
  }

  algo_frame = (s_goodix.frame++ % s_goodix.decimation) == 0U;

  if (s_goodix.hrv) {
    prv_process_hrv(frame);
  }

  if (!algo_frame) {
    return;
  }

  if (s_goodix.nadt) {
    prv_process_nadt(frame);
  }

  if (s_goodix.hr) {
    prv_process_hr(frame);
  }

  if (s_goodix.spo2) {
    prv_process_spo2(frame);
  }
}

static void prv_set_scene(HRMActivityScene scene) {
  s_goodix.scene = (scene < ARRAY_LENGTH(s_scenes)) ? s_scenes[scene] : HBA_SCENES_DEFAULT;
}

const struct hrm_ppg_backend HRM_PPG_BACKEND = {
  .start = prv_start,
  .stop = prv_stop,
  .process = prv_process,
  .set_scene = prv_set_scene,
};
