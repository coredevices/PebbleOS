#include "drivers/otp.h"

#include "system/passert.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

static char slot[32];

char * otp_get_slot(const uint8_t index) {
  return slot;
}

uint8_t * otp_get_lock(const uint8_t index) {
  return (uint8_t *)slot;
}

bool otp_is_locked(const uint8_t index) {
  return false;
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  return OtpWriteFailAlreadyWritten;
}
