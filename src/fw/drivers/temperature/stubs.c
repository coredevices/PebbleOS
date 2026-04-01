#include <inttypes.h>

#include "drivers/temperature.h"
#include "console/prompt.h"

void temperature_init(void) {

}

int32_t temperature_read(void) {
  return 0;
}

void command_temperature_read(void) {
  char buffer[32];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRId32" ", temperature_read());
}
