#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "constants_worker.h"
#include "util/trig.h"
#include "system/logging.h"


int16_t pow_int(int16_t x, int16_t y);


uint32_t isqrt(uint32_t x);

int32_t integral_abs(int16_t *d, int16_t srti, int16_t endi);

int32_t integral_l2(int16_t *d, int16_t srti, int16_t endi);

uint8_t get_angle_i(int16_t x, int16_t y, uint8_t n_ang );

// actigraphy functions

uint8_t orient_encode(int16_t *mean_ary, uint8_t n_ang);

void fft_mag(int16_t *d, int16_t dlenpwr);


