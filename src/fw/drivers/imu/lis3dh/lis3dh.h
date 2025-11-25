/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "applib/accel_service.h"

static const int16_t LIS3DH_COUNTS_PER_G = 4096;
static const int16_t LIS3DH_SAMPLING_RATE_HZ = 50;

static const int LIS3DH_MIN_VALUE = -32768;
static const int LIS3DH_MAX_VALUE = 32767;
static const uint8_t LIS3DH_WHOAMI_BYTE = 0x33;

// Computing AccelSamplingRate * LIS3DH_TIME_LIMIT_MULT / LIS3DH_TIME_LIMIT_DIV
// yields the correct setting for the TIME_LIMIT register
static const int LIS3DH_TIME_LIMIT_MULT = 2240;
static const int LIS3DH_TIME_LIMIT_DIV = 1000;
// Computing AccelSamplingRate * LIS3DH_TIME_LATENCY_MULT / LIS3DH_TIME_LATENCY_DIV
// yields the correct setting for the TIME_LIMIT register
static const int LIS3DH_TIME_LATENCY_MULT = 1280;
static const int LIS3DH_TIME_LATENCY_DIV = 1000;
// Computing AccelSamplingRate * LIS3DH_TIME_WINDOW_MULT / LIS3DH_TIME_WINDOW_DIV
// yields the correct setting for the TIME_WINDOW register
static const int LIS3DH_TIME_WINDOW_MULT = 5120;
static const int LIS3DH_TIME_WINDOW_DIV = 1000;
// Computing AccelScale * LIS3DH_THRESHOLD_MULT / LIS3DH_THRESHOLD_DIV
// yields the correct setting for the CLICK_THS register
static const int LIS3DH_THRESHOLD_MULT = 24;
static const int LIS3DH_THRESHOLD_DIV = 1;

typedef enum {
  SELF_TEST_MODE_OFF,
  SELF_TEST_MODE_ONE,
  SELF_TEST_MODE_TWO,
  SELF_TEST_MODE_COUNT
} SelfTestMode;

//! Valid accelerometer scales, in g's
typedef enum {
  LIS3DH_SCALE_UNKNOWN = 0,
  LIS3DH_SCALE_16G = 16,
  LIS3DH_SCALE_8G = 8,
  LIS3DH_SCALE_4G = 4,
  LIS3DH_SCALE_2G = 2,
} Lis3dhScale;

void lis3dh_init(void);

void lis3dh_lock(void);
void lis3dh_unlock(void);
void lis3dh_init_mutex(void);

void enable_lis3dh_interrupts(void);
void disable_lis3dh_interrupts(void);

bool lis3dh_sanity_check(void);

// Poke specific registers
void lis3dh_disable_click(void);
void lis3dh_enable_click(void);

void lis3dh_disable_fifo(void);
void lis3dh_enable_fifo(void);
bool lis3dh_is_fifo_enabled(void);

void lis3dh_power_up(void);
void lis3dh_power_down(void);

void lis3dh_set_interrupt_axis(AccelAxisType axis, bool double_click);

uint8_t lis3dh_get_interrupt_threshold();
static const uint8_t LIS3DH_MAX_THRESHOLD = 0x7f;
void lis3dh_set_interrupt_threshold(uint8_t threshold);

uint8_t lis3dh_get_interrupt_time_limit();
static const uint8_t LIS3DH_MAX_TIME_LIMIT = 0x7f;
void lis3dh_set_interrupt_time_limit(uint8_t time_limit);

uint8_t lis3dh_get_click_latency();
static const uint8_t LIS3DH_MAX_CLICK_LATENCY = 0xff;
void lis3dh_set_click_latency(uint8_t latency);

uint8_t lis3dh_get_click_window();
static const uint8_t LIS3DH_MAX_CLICK_WINDOW = 0xff;
void lis3dh_set_click_window(uint8_t window);

bool lis3dh_set_fifo_mode(uint8_t);
uint8_t lis3dh_get_fifo_mode(void);

bool lis3dh_set_fifo_wtm(uint8_t);
uint8_t lis3dh_get_fifo_wtm(void);


bool lis3dh_enter_self_test_mode(SelfTestMode mode);
void lis3dh_exit_self_test_mode(void);
bool lis3dh_self_test(void);

bool lis3dh_config_set_defaults();

bool lis3dh_read(uint8_t address, uint8_t read_size, uint8_t *buffer);
bool lis3dh_write(uint8_t address, uint8_t write_size, const uint8_t *buffer);
