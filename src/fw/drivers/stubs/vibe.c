#include <stdlib.h>

#include "drivers/vibe.h"
#include "console/prompt.h"

void vibe_init(void) {
}

void vibe_set_strength(int8_t strength) {
}

void vibe_ctl(bool on) {
}

void vibe_force_off(void) {
}

int8_t vibe_get_braking_strength(void) {
  // We only support the 0..100 range, just ask it to turn off
  return VIBE_STRENGTH_OFF;
}


void command_vibe_ctl(const char *arg) {
  int strength = atoi(arg);

  const bool out_of_bounds = ((strength < 0) || (strength > VIBE_STRENGTH_MAX));
  const bool not_a_number = (strength == 0 && arg[0] != '0');
  if (out_of_bounds || not_a_number) {
    prompt_send_response("Invalid argument");
    return;
  }

  vibe_set_strength(strength);

  const bool turn_on = strength != 0;
  vibe_ctl(turn_on);
  prompt_send_response("OK");
}
