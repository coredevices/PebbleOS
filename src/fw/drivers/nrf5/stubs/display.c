#include "applib/graphics/gtypes.h"
#include "board/board.h"
#include "debug/power_tracking.h"
#include "kernel/util/sleep.h"
#include "kernel/util/stop.h"
#include "os/tick.h"
#include "services/common/analytics/analytics.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/bitset.h"
#include "util/net.h"
#include "util/reverse.h"
#include "util/units.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t display_baud_rate_change(uint32_t new_frequency_hz) {
  return new_frequency_hz;
}

void display_init(void) {
}

void display_clear(void) {
}

bool display_update_in_progress(void) {
  return true;
}

void display_update(NextRowCallback nrcb, UpdateCompleteCallback uccb) {
  PBL_ASSERTN(nrcb != NULL);
  PBL_ASSERTN(uccb != NULL);
}

void display_pulse_vcom(void) {
  PBL_ASSERTN(BOARD_CONFIG.lcd_com.gpio != 0);
}

void display_show_splash_screen(void) {
  // The bootloader has already drawn the splash screen for us; nothing to do!
}

// Stubs for display offset
void display_set_offset(GPoint offset) {}

GPoint display_get_offset(void) { return GPointZero; }
