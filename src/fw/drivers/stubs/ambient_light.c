#include <inttypes.h>

#include "drivers/ambient_light.h"
#include "console/prompt.h"

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
