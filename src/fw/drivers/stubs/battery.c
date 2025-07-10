#include "drivers/battery.h"

void battery_init(void) {
}

bool battery_is_present(void) {
  return true;
}

int battery_get_millivolts(void) {
  return 4000;
}

int battery_get_constants(BatteryConstants *constants) {
  constants->v_mv = 4000;
  constants->i_ua = 100;
  constants->t_mc = 25000;
  return 0;
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
