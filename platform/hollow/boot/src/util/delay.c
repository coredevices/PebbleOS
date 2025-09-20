#include "delay.h"

#include <nrfx.h>

void delay_us(uint32_t us) {
  NRFX_DELAY_US(us);
}

void delay_ms(uint32_t millis) {
  // delay_us(millis*1000) is not used because a long delay could easily
  // overflow the veriable. Without the outer loop, a delay of even five
  // seconds would overflow.
  while (millis--) {
    delay_us(1000);
  }
}
