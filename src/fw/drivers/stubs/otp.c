#include "drivers/otp.h"

static char slot[32];

char * otp_get_slot(const uint8_t index) {
  return slot;
}

uint8_t * otp_get_lock(const uint8_t index) {
  return (uint8_t *)slot;
}

bool otp_is_locked(const uint8_t index) {
  return true;
}

OtpWriteResult otp_write_slot(const uint8_t index, const char *value) {
  return OtpWriteFailAlreadyWritten;
}
