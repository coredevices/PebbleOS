/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "math.h"

#include "gh3x2x.h"

#include "drivers/hrm.h"
#include "board/board.h"
#include "kernel/util/sleep.h"
#include "services/common/system_task.h"
#include "services/common/hrm/hrm_manager.h"
#include "system/logging.h"

#include "gh_demo.h"
#include "gh_demo_inner.h"

#define GH3X2X_LOG_ENABLE 0
#define GH3X2X_FIFO_WATERMARK_CONFIG 80
#define GH3X2X_HR_SAMPLING_RATE 25

static volatile uint32_t s_hrm_int_flag = false;

// GH3X2X library glue code

void gh3026_i2c_init(void) {}

void gh3026_i2c_write(uint8_t device_id, const uint8_t write_buffer[], uint16_t length) {
  i2c_use(HRM->i2c);
  i2c_write_block(HRM->i2c, length, write_buffer);
  i2c_release(HRM->i2c);
}

void gh3026_i2c_read(uint8_t device_id, const uint8_t write_buffer[], uint16_t write_length,
                     uint8_t read_buffer[], uint16_t read_length) {
  i2c_use(HRM->i2c);
  i2c_write_block(HRM->i2c, write_length, write_buffer);
  i2c_read_block(HRM->i2c, read_length, read_buffer);
  i2c_release(HRM->i2c);
}

void gh3026_reset_pin_init(void) {}

void gh3026_reset_pin_ctrl(uint8_t pin_level) {
#if GH3X2X_RESET_PIN_CTRLBY_NPM1300
  NPM1300_OPS.gpio_set(Npm1300_Gpio3, pin_level);
  psleep(10);
#endif
}

void gh3026_gsensor_data_get(STGsensorRawdata gsensor_buffer[], GU16 *gsensor_buffer_index) {
  // TODO, clean the buffer now
  GU16 count = *gsensor_buffer_index;
  for (uint16_t i = 0; i < count; ++i) {
    memset(&gsensor_buffer[i], 0, sizeof(STGsensorRawdata));
  }
}

static void gh3026_int_callback_function(void *context) {
  s_hrm_int_flag = false;
  Gh3x2xDemoInterruptProcess();
}

static void gh3026_int_irq_callback(bool *should_context_switch) {
  hal_gh3x2x_int_handler_call_back();

  if (s_hrm_int_flag == false) {
    if (system_task_add_callback_from_isr(gh3026_int_callback_function, NULL,
                                          should_context_switch)) {
      s_hrm_int_flag = true;
    }
  } else {
    *should_context_switch = false;
  }
}

void gh3026_int_pin_init(void) {
  exti_configure_pin(HRM->int_exti, ExtiTrigger_Rising, gh3026_int_irq_callback);
  exti_enable(HRM->int_exti);
}

void gh3x2x_print_fmt(const char *fmt, ...) {
#if GH3X2X_LOG_ENABLE
  char buffer[128];
  va_list ap;
  va_start(ap, fmt);
  vsniprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);
  PBL_LOG(LOG_LEVEL_ALWAYS, "%s", buffer);
#endif
}

void gh3x2x_result_report(uint8_t type, uint32_t val, uint8_t quality) {
  if (type == 1) {
    HRMData hrm_data = {0};

    PBL_LOG(LOG_LEVEL_DEBUG, "GH3X2X BPM %" PRIu32 " (quality=%" PRIu8 ")", val, quality);

    hrm_data.features = HRMFeature_BPM;
    hrm_data.hrm_bpm = val & 0xff;

    if (quality == 254U) {
      hrm_data.hrm_quality = HRMQuality_OffWrist;
    } else if (quality >= 80U) {
      hrm_data.hrm_quality = HRMQuality_Excellent;
    } else if (quality >= 70U) {
      hrm_data.hrm_quality = HRMQuality_Good;
    } else if (quality >= 60U) {
      hrm_data.hrm_quality = HRMQuality_Acceptable;
    } else if (quality >= 50U) {
      hrm_data.hrm_quality = HRMQuality_Poor;
    } else if (quality >= 30U) {
      hrm_data.hrm_quality = HRMQuality_Worst;
    } else {
      hrm_data.hrm_quality = HRMQuality_NoSignal;
    }

    hrm_manager_new_data_cb(&hrm_data);
  } else if (type == 2) {
    HRMData hrm_data = {0};

    PBL_LOG(LOG_LEVEL_DEBUG, "GH3X2X SpO2 %" PRIu32 " (quality=%" PRIu8 ")", val, quality);

    hrm_data.features = HRMFeature_SpO2;
    hrm_data.spo2_percent = val & 0xff;

    // FIXME(GH3X2X): This mapping is wrong, we need to understand the actual quality values
    if (quality == 254U) {
      hrm_data.spo2_quality = HRMQuality_OffWrist;
    } else if (quality >= 80U) {
      hrm_data.spo2_quality = HRMQuality_Excellent;
    } else if (quality >= 70U) {
      hrm_data.spo2_quality = HRMQuality_Good;
    } else if (quality >= 60U) {
      hrm_data.spo2_quality = HRMQuality_Acceptable;
    } else if (quality >= 50U) {
      hrm_data.spo2_quality = HRMQuality_Poor;
    } else if (quality >= 30U) {
      hrm_data.spo2_quality = HRMQuality_Worst;
    } else {
      hrm_data.spo2_quality = HRMQuality_NoSignal;
    }

    hrm_manager_new_data_cb(&hrm_data);
  } else {
    PBL_LOG(LOG_LEVEL_WARNING, "GH3X2X unexpected report type (%" PRIu8 ")", type);
  }
}

// HRM interface

void hrm_init(HRMDevice *dev) {
  int ret;

  ret = Gh3x2xDemoInit();
  if (ret != 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "GH3X2X failed to initialize");
    return;
  }

  dev->state->initialized = true;

  hrm_disable(dev);
}

void hrm_enable(HRMDevice *dev) {
  if (!dev->state->initialized) {
    return;
  }

  s_hrm_int_flag = false;
  dev->state->enabled = true;

  GH3X2X_FifoWatermarkThrConfig(GH3X2X_FIFO_WATERMARK_CONFIG);
  GH3X2X_SetSoftEvent(GH3X2X_SOFT_EVENT_NEED_FORCE_READ_FIFO);
  Gh3x2xDemoFunctionSampleRateSet(GH3X2X_FUNCTION_HR, GH3X2X_HR_SAMPLING_RATE);
  Gh3x2xDemoStartSampling(GH3X2X_FUNCTION_HR | GH3X2X_FUNCTION_SPO2);
}

void hrm_disable(HRMDevice *dev) {
  if (!dev->state->initialized) {
    return;
  }

  Gh3x2xDemoStopSampling(0xFFFFFFFF);
  dev->state->enabled = false;
}

bool hrm_is_enabled(HRMDevice *dev) {
  return dev->state->enabled;
}
