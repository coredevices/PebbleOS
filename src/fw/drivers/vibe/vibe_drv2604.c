#include "drivers/vibe.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/battery/battery_state.h"
#include "services/common/analytics/analytics.h"

#include <string.h>

/* XXX: tune RATED_VOLTAGE? / OD_CLAMP? */

#define DRV2604_STATUS        0x00
#define DRV2604_MODE          0x01
#define DRV2604_MODE_TRIGGER 0x00
#define DRV2604_MODE_RTP     0x05
#define DRV2604_MODE_AUTOCAL 0x07
#define DRV2604_MODE_STANDBY 0x40
#define DRV2604_RTP_INPUT     0x02
#define DRV2604_GO            0x0C
#define DRV2604_RATED_VOLTAGE 0x16
#define DRV2604_OD_CLAMP      0x17
#define DRV2604_A_CAL_COMP    0x18
#define DRV2604_A_CAL_BEMF    0x19
#define DRV2604_FBCTL         0x1A
#define DRV2604_FBCTL_LRA                0x80
#define DRV2604_FBCTL_FB_BRAKE_FACTOR(n) ((n) << 4)
#define DRV2604_FBCTL_LOOP_GAIN(n)       ((n) << 2)
#define DRV2604_FBCTL_BEMF_GAIN(n)       ((n) << 0)
#define DRV2604_CONTROL1      0x1B
#define DRV2604_CONTROL1_STARTUP_BOOST 0x80
#define DRV2604_CONTROL1_DRIVE_TIME(n) ((n) << 0)
#define DRV2604_CONTROL2      0x1C
#define DRV2604_CONTROL2_BIDIR_INPUT      0x80
#define DRV2604_CONTROL2_BRAKE_STABILIZER 0x40
#define DRV2604_CONTROL2_SAMPLE_TIME(n)   ((n) << 4)
#define DRV2604_CONTROL2_BLANKING_TIME(n) ((n) << 2)
#define DRV2604_CONTROL2_IDISS_TIME(n) ((n) << 0)
#define DRV2604_CONTROL4      0x1E
#define DRV2604_CONTROL4_AUTO_CAL_TIME(n) ((n) << 4)

static bool s_initialized = false;

static bool prv_read_register(uint8_t register_address, uint8_t *result) {
  i2c_use(I2C_DRV2604);
  bool rv = i2c_read_register(I2C_DRV2604, register_address, result);
  i2c_release(I2C_DRV2604);
  return rv;
}

static bool prv_write_register(uint8_t register_address, uint8_t datum) {
  i2c_use(I2C_DRV2604);
  uint8_t block[2] = { register_address, datum };
  bool rv = i2c_write_block(I2C_DRV2604, 2, block);
  i2c_release(I2C_DRV2604);
  return rv;
}

void vibe_init(void) {
  gpio_output_init(&BOARD_CONFIG_VIBE.ctl, GPIO_OType_PP, GPIO_Speed_2MHz);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  uint8_t rv;
  bool found = prv_read_register(DRV2604_STATUS, &rv);
  if (found) {
    PBL_LOG(LOG_LEVEL_INFO, "Found DRV2604 with STATUS register %02x", rv);
  } else {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to read the STATUS register");
  }
  
  /* calibration table maybe should live in the board file? */
  const uint8_t regs[][2] = {
    { DRV2604_MODE, DRV2604_MODE_TRIGGER },
    { DRV2604_FBCTL, DRV2604_FBCTL_LRA | DRV2604_FBCTL_FB_BRAKE_FACTOR(2) | DRV2604_FBCTL_LOOP_GAIN(2) | DRV2604_FBCTL_BEMF_GAIN(2) },
    { DRV2604_RATED_VOLTAGE, 0x3F /* default */ },
    { DRV2604_OD_CLAMP, 0x89 /* default */ },
    { DRV2604_A_CAL_COMP, 0x0D },
    { DRV2604_A_CAL_BEMF, 0x80 },
    { DRV2604_CONTROL1, DRV2604_CONTROL1_STARTUP_BOOST | DRV2604_CONTROL1_DRIVE_TIME(0x10 /* 2.1 ms */) },
    { DRV2604_CONTROL2, DRV2604_CONTROL2_BIDIR_INPUT | DRV2604_CONTROL2_BRAKE_STABILIZER | DRV2604_CONTROL2_SAMPLE_TIME(3) | DRV2604_CONTROL2_BLANKING_TIME(1) | DRV2604_CONTROL2_IDISS_TIME(1) },
    { DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_TRIGGER },
  };

  for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
    if (!prv_write_register(regs[i][0], regs[i][1])) {
      PBL_LOG(LOG_LEVEL_ERROR, "failed to write register %02x on DRV2604", regs[i][0]);
      gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
      return;
    }
  }

  // DRV2604 does not get its registers reset by disabling EN, so it's ok to
  // do that
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  
  s_initialized = true;
}

static bool s_vibe_ctl_on = false;

/* Sadly, you cannot play music with DRV2604 this way.  Maybe we should
 * modulate DRIVE_TIME too?
 */
void vibe_set_strength(int8_t strength) {
  int32_t strength_scale = strength * 0x7FL / 100L;
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, true);
  prv_write_register(DRV2604_MODE, DRV2604_MODE_RTP); /* exit standby, RTP mode */
  prv_write_register(DRV2604_RTP_INPUT, strength_scale);
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, s_vibe_ctl_on);
}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on && battery_monitor_critical_lockout()) {
    on = false;
  }

  static bool s_on = false;
  if (on && !s_on) {
    analytics_inc(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_COUNT, AnalyticsClient_System);
    analytics_stopwatch_start(ANALYTICS_APP_METRIC_VIBRATOR_ON_TIME, AnalyticsClient_App);
    analytics_stopwatch_start(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_TIME, AnalyticsClient_System);
  } else if (!on && s_on) {
    analytics_stopwatch_stop(ANALYTICS_APP_METRIC_VIBRATOR_ON_TIME);
    analytics_stopwatch_stop(ANALYTICS_DEVICE_METRIC_VIBRATOR_ON_TIME);
  }
  s_on = on;

  PBL_LOG(LOG_LEVEL_DEBUG, "Vibe status <%s>", on ? "on" : "off");

  if (!on) {
    prv_write_register(DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_RTP); /* enter standby even if the enable GPIO is not hooked up */
  }
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, on);
  s_vibe_ctl_on = on;
  if (on) {
    prv_write_register(DRV2604_MODE, DRV2604_MODE_RTP); /* exit standby, RTP mode */
  }
}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }
  prv_write_register(DRV2604_MODE, DRV2604_MODE_STANDBY | DRV2604_MODE_RTP); /* enter standby even if the enable GPIO is not hooked up */
  gpio_output_set(&BOARD_CONFIG_VIBE.ctl, false);
  s_vibe_ctl_on = false;
}

int8_t vibe_get_braking_strength(void) {
  // We support the -100..100 range because BIDIR_INPUT is set
  return VIBE_STRENGTH_MIN;
}

status_t vibe_calibrate(void) {
  bool bad = false;
  bad |= !prv_write_register(DRV2604_MODE, DRV2604_MODE_AUTOCAL);
  bad |= !prv_write_register(DRV2604_FBCTL, DRV2604_FBCTL_LRA | DRV2604_FBCTL_FB_BRAKE_FACTOR(2) | DRV2604_FBCTL_LOOP_GAIN(2));
  bad |= !prv_write_register(DRV2604_RATED_VOLTAGE, 0x3F); /* default value */
  bad |= !prv_write_register(DRV2604_OD_CLAMP,      0x89); /* default value */
  bad |= !prv_write_register(DRV2604_CONTROL1, DRV2604_CONTROL1_STARTUP_BOOST | DRV2604_CONTROL1_DRIVE_TIME(0x10 /* 2.1 ms */));
  bad |= !prv_write_register(DRV2604_CONTROL2, DRV2604_CONTROL2_BIDIR_INPUT | DRV2604_CONTROL2_BRAKE_STABILIZER | DRV2604_CONTROL2_SAMPLE_TIME(3) | DRV2604_CONTROL2_BLANKING_TIME(1) | DRV2604_CONTROL2_IDISS_TIME(1));
  bad |= !prv_write_register(DRV2604_CONTROL4, DRV2604_CONTROL4_AUTO_CAL_TIME(3));
  bad |= !prv_write_register(DRV2604_GO, 1); /* GO */

  return bad ? E_ERROR : S_SUCCESS;
}

void command_vibe_ctl(const char *arg) {
  if (!strcmp(arg, "cal")) {
    status_t res;
    char buf[64];

    prompt_send_response("vibe cal...");

    res = vibe_calibrate();
    if (res != S_SUCCESS) {
      prompt_send_response_fmt(buf, 64, "vibe cal failed");
    } else {
      prompt_send_response_fmt(buf, 64, "vibe cal succeeded");
    }

    return;
  }
  
  if (!strcmp(arg, "reg")) {
    prompt_send_response("vibe regs:");
    for (int i = 0; i <= 0x22; i++) {
      uint8_t reg;
      char buf[64];
      prv_read_register(i, &reg);
      prompt_send_response_fmt(buf, 64, "  vibe reg %02x: %02x", i, reg);
    }
    return;
  }

  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}
