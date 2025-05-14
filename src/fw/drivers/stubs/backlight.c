#include "drivers/backlight.h"

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
