/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "gh3x2x.h"
#include "drivers/hrm.h"
#include "board/board.h"
#include "drivers/rtc.h"
#include "kernel/events.h"
#include "kernel/util/sleep.h"
#include "system/logging.h"
#include "os/tick.h"
#include "services/common/system_task.h"
#include "math.h"
#include "gh_demo.h"
#include "gh_demo_inner.h"

#define GH3X2X_LOG_ENABLE                       (0)
#define GH3X2X_FIFO_WATERMARK_CONFIG            (80)
#define GH3X2X_HR_SAMPLING_RATE                 (25)

enum GH3X2XSQIThreshold {
  GH3X2XSQIThreshold_Excellent = 80,
  GH3X2XSQIThreshold_Good = 70,
  GH3X2XSQIThreshold_Acceptable = 60,
  GH3X2XSQIThreshold_Poor = 50,
  GH3X2XSQIThreshold_Worst = 30,

  GH3X2XSQIThreshold_OffWrist = 254,

  GH3X2XSQIThresholdInvalid,
};

static volatile uint32_t m_hrm_int_flag = false;

void gh3026_i2c_init(void) {}

// implement supported math function for algo libs
float log10f(float x)
{
    return logf(x) / logf(10.0f);
}

float tanhf(float x)
{
    return (expf(x) - expf(-x)) / (expf(x) + expf(-x));
}

void gh3026_i2c_write(uint8_t device_id, const uint8_t write_buffer[], uint16_t length) {
  i2c_use(HRM->i2c);
  i2c_write_block(HRM->i2c, length, write_buffer);
  i2c_release(HRM->i2c);
}

void gh3026_i2c_read(uint8_t device_id, const uint8_t write_buffer[], uint16_t write_length, uint8_t read_buffer[], uint16_t read_length) {
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
  //TODO, clean the buffer now
  GU16 count = *gsensor_buffer_index;
  for (uint16_t i=0; i<count; ++i) {
    memset(&gsensor_buffer[i], 0, sizeof(STGsensorRawdata));
  }
}

static void gh3026_int_callback_function(void *context) {
  m_hrm_int_flag = false;
  uint32_t time_ms = ticks_to_milliseconds(rtc_get_ticks());
  Gh3x2xDemoInterruptProcess();
}

static void gh3026_int_irq_callback(bool *should_context_switch) {
  uint32_t time_ms = ticks_to_milliseconds(rtc_get_ticks());
  hal_gh3x2x_int_handler_call_back();
  
  if (m_hrm_int_flag == false) {
    if (system_task_add_callback_from_isr(gh3026_int_callback_function, NULL, should_context_switch)) {
      m_hrm_int_flag = true;
    }
  } else {
    if (should_context_switch) {
      *should_context_switch = true;
    }
  }
}

void gh3026_int_pin_init(void) {
  exti_configure_pin(HRM->int_exti, ExtiTrigger_Rising, gh3026_int_irq_callback);
  exti_enable(HRM->int_exti);
}

void hrm_init(HRMDevice *dev) {
  int32_t ret = Gh3x2xDemoInit();
  if (ret == 0) {
    Gh3x2xDemoStopSampling(0xFFFFFFFF);
    dev->state->enabled = false;
  }
}

void hrm_enable(HRMDevice *dev) {
  m_hrm_int_flag = false;
  dev->state->enabled = true;
  GH3X2X_FifoWatermarkThrConfig(GH3X2X_FIFO_WATERMARK_CONFIG);
  GH3X2X_SetSoftEvent(GH3X2X_SOFT_EVENT_NEED_FORCE_READ_FIFO);
  Gh3x2xDemoFunctionSampleRateSet(GH3X2X_FUNCTION_HR, GH3X2X_HR_SAMPLING_RATE);
  Gh3x2xDemoStartSampling(GH3X2X_FUNCTION_HR);
}

void hrm_disable(HRMDevice *dev) {
  Gh3x2xDemoStopSampling(0xFFFFFFFF);
  dev->state->enabled = false;
}

bool hrm_is_enabled(HRMDevice *dev) {
  return dev->state->enabled;
}

void gh3x2x_print_fmt(const char* fmt, ...) {
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
    
    /*PebbleHRMEvent hrm_event = {
      .event_type = HRMEvent_BPM,
      .bpm = {
        .bpm = val & 0xff,
        .quality = HRMQuality_Good,
      },
    };*/
    
    PBL_LOG(LOG_LEVEL_DEBUG, "gh3026 bpm(%ld), quality(%d)", val, quality);

    HRMData hrm_data = {0};
    hrm_data.hrm_bpm = val & 0xff;
    hrm_data.hrm_quality = quality;
    hrm_manager_new_data_cb(&hrm_data);
  } else if (type == 2) {
    //TODO: for SPO2
  }
}

