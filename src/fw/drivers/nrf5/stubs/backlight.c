#include "drivers/backlight.h"

#include <string.h>
#include <stdlib.h>

#include "board/board.h"
#include "debug/power_tracking.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/led_controller.h"
#include "drivers/periph_config.h"
#include "drivers/pwm.h"
#include "drivers/timer.h"
#include "kernel/util/stop.h"
#include "system/logging.h"
#include "system/passert.h"

void backlight_init(void) {
}

// TODO: PBL-36077 Move to a generic 4v5 enable
void led_enable(LEDEnabler enabler) {
}

// TODO: PBL-36077 Move to a generic 4v5 disable
void led_disable(LEDEnabler enabler) {
}

void backlight_set_brightness(uint16_t brightness) {
}

void command_backlight_ctl(const char *arg) {
}
