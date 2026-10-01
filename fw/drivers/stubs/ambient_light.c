#include <inttypes.h>

#include <pbl/drivers/ambient_light.h>

void ambient_light_init(void) {
}

void ambient_light_prime(void) {
}

void ambient_light_release(void) {
}

void ambient_light_suspend(void) {
}

void ambient_light_resume(void) {
}

uint32_t ambient_light_get_light_level(void) {
  return 0;
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

bool ambient_light_lux_available(void) {
  return false;
}

uint32_t ambient_light_level_to_lux(uint32_t light_level) {
  return light_level;
}

#ifdef CONFIG_SHELL
#include <pbl/shell/shell.h>

static int prv_cmd_als_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "%" PRIu32, ambient_light_get_light_level());
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_als, read, NULL, "Read the raw light level", prv_cmd_als_read, 0, 0);
#endif
