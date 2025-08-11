#include "drivers/vibe.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/i2c.h"
#include "drivers/periph_config.h"
#include "drivers/pmic.h"
#include "drivers/pwm.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/math.h"

#include "services/common/analytics/analytics.h"
#include "services/common/battery/battery_monitor.h"
#include "services/common/battery/battery_state.h"
#include "services/common/analytics/analytics.h"

#include <string.h>
#include "haptic_nv.h"

static bool s_initialized = false;


void vibe_init(void) {
  //gpio_output_init(&BOARD_CONFIG_VIBE.ctrl, GPIO_OType_PP, GPIO_Speed_2MHz);
  //gpio_output_set(&BOARD_CONFIG_VIBE.ctrl, true);
 
  haptic_nv_boot_init();
  //haptic_nv_play_start();
  //PBL_LOG(LOG_LEVEL_DEBUG, "get f0:%d\n", g_func_haptic_nv->get_f0());
  //g_func_haptic_nv->get_lra_resistance();
  g_func_haptic_nv->short_vib_work(1, 0x80, 2);
  //g_func_haptic_nv->long_vib_work(1, 0x80, 5000);
  //haptic_nv_play_start();

  

  //gpio_output_set(&BOARD_CONFIG_VIBE.ctrl, false);
  s_initialized = true;
}

static bool s_vibe_ctl_on = false;

/* Sadly, you cannot play music with DRV2604 this way.  Maybe we should
 * modulate DRIVE_TIME too?
 */
void vibe_set_strength(int8_t strength) {


}

void vibe_ctl(bool on) {
  if (!s_initialized) {
    return;
  }

  if (on) {
    PBL_LOG(LOG_LEVEL_DEBUG, "vibe ctrl on");
    //haptic_nv_play_start();
    g_func_haptic_nv->short_vib_work(1, 0x80, 14);
  } else {
    PBL_LOG(LOG_LEVEL_DEBUG, "vibe ctrl off");
    haptic_nv_play_stop();
  }

}

void vibe_force_off(void) {
  if (!s_initialized) {
    return;
  }

}

int8_t vibe_get_braking_strength(void) {
  // We support the -100..100 range because BIDIR_INPUT is set
  return VIBE_STRENGTH_MIN;
}


void command_vibe_ctl(const char *arg) {
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
