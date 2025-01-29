#include "board/board.h"
#include "console/prompt.h"
#include "drivers/ambient_light.h"
#include "drivers/gpio.h"
#include "drivers/voltage_monitor.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>

void ambient_light_init(void) {
}

uint32_t ambient_light_get_light_level(void) {
  return 0;
}

void command_als_read(void) {
  char buffer[16];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRIu32"", ambient_light_get_light_level());
}

uint32_t ambient_light_get_dark_threshold(void) {
  return 1;
}

void ambient_light_set_dark_threshold(uint32_t new_threshold) {
}

bool ambient_light_is_light(void) {
  return false;
}

AmbientLightLevel ambient_light_level_to_enum(uint32_t light_level) {
  return AmbientLightLevelUnknown;
}
