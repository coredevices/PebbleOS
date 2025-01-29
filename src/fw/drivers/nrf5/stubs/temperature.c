#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/temperature.h"
#include "drivers/voltage_monitor.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "services/common/regular_timer.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>

void temperature_init(void) {

}

int32_t temperature_read(void) {
  return 0;
}

void command_temperature_read(void) {
  char buffer[32];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRId32" ", temperature_read());
}
