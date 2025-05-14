#include "drivers/accel.h"

static uint32_t s_sampling_interval_ms;

uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  s_sampling_interval_ms = interval_us / 1000;
  return accel_get_sampling_interval();
}

uint32_t accel_get_sampling_interval(void) {
  return s_sampling_interval_ms * 1000;
}


void accel_set_num_samples(uint32_t num_samples) {
}


int accel_peek(AccelDriverSample *data) {
  return -1;
}


void accel_enable_shake_detection(bool on) {
}


bool accel_get_shake_detection_enabled(void) {
  return false;
}


void accel_enable_double_tap_detection(bool on) {
}


bool accel_get_double_tap_detection_enabled(void) {
  return false;
}


void accel_set_shake_sensitivity_high(bool sensitivity_high) {
}

bool accel_run_selftest(void) {
  return true;
}

void command_accel_status(void) {
}

void command_accel_selftest(void) {
}

void command_accel_softreset(void) {
}
