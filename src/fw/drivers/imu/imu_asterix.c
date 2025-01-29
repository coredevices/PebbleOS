#include "board/board.h"

#include "drivers/accel.h"
#include "drivers/imu.h"
#include "drivers/spi.h"
#include "kernel/util/delay.h"

#include "drivers/imu/bmm350/bmm350.h"

void imu_init(void) {
  /* no IMU on asterix */
}

void imu_power_up(void) {
  // NYI
}

void imu_power_down(void) {
  // NYI
}

