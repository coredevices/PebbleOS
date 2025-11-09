#if PLATFORM_ASTERIX
#include "services/normal/orientation_manager.h"
#include "system/passert.h"
#include "shell/prefs.h"
#include "drivers/display/display.h"
#include "drivers/button.h"
#include "drivers/imu/lsm6dso/lsm6dso.h"
#include "drivers/imu/mmc5603nj/mmc5603nj.h"

void prv_change_orientation(bool rotated) {
  display_set_rotated(rotated);
  button_set_rotated(rotated);
  imu_set_rotated(rotated);
  mag_set_rotated(rotated);
}

void orientation_handle_prefs_changed(void) {
  prv_change_orientation(display_orientation_is_left());
}

void orientation_manager_enable(bool on) {
  prv_change_orientation(on ? display_orientation_is_left() : false);
}
#endif