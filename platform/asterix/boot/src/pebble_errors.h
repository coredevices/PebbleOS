#pragma once

#include <stdint.h>

static const uint32_t ERROR_NO_ACTIVE_ERROR = 0;

// FW Errors
static const uint32_t ERROR_STUCK_BUTTON  = 0xfe504501;
static const uint32_t ERROR_BAD_SPI_FLASH = 0xfe504502;
static const uint32_t ERROR_CANT_LOAD_FW = 0xfe504503;
static const uint32_t ERROR_RESET_LOOP = 0xfe504504;

// BT Errors
static const uint32_t ERROR_CANT_LOAD_BT = 0xfe504510;
static const uint32_t ERROR_CANT_START_LSE = 0xfe504511;
static const uint32_t ERROR_CANT_START_ACCEL = 0xfe504512;

static const uint32_t ERROR_LOW_BATTERY = 0xfe504520;
