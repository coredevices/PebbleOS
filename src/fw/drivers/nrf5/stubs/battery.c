#include "drivers/battery.h"

#include "system/passert.h"
#include "services/common/battery/battery_state.h"
#include "services/common/battery/battery_curve.h"
#include "system/logging.h"

#include "util/math.h"
#include "util/net.h"

void battery_init(void) {
}

int battery_get_millivolts(void) {
  return 4000;
}

bool battery_charge_controller_thinks_we_are_charging_impl(void) {
  return 1;
}

bool battery_is_usb_connected_impl(void) {
  return 1;
}

void battery_set_charge_enable(bool charging_enabled) {
}

void battery_set_fast_charge(bool fast_charge_enabled) {
}
