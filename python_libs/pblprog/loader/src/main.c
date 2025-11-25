/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#define HEADER_ADDR (0x20000400)
#define DATA_ADDR   (0x20000800)
#define FLASH_SR_ADDR (0x40023C0C)

#define STATE_WAITING (0)
#define STATE_WRITE (1)
#define STATE_CRC (2)

// typedef to make it easy to change the program size
typedef uint8_t p_size_t;

typedef struct __attribute__((__packed__)) {
  uint32_t state;
  volatile p_size_t *addr;
  uint32_t length;
} Header;

static uint8_t prv_crc8(const uint8_t *data, uint32_t data_len) {
  uint8_t crc = 0;

  // nibble lookup table for (x^8 + x^5 + x^3 + x^2 + x + 1)
  static const uint8_t lookup_table[] =
    { 0, 47, 94, 113, 188, 147, 226, 205, 87, 120, 9, 38, 235, 196, 181, 154 };

  for (uint32_t i = 0; i < data_len * 2; i++) {
    uint8_t nibble = data[i / 2];

    if (i % 2 == 0) {
      nibble >>= 4;
    }

    uint8_t index = nibble ^ (crc >> 4);
    crc = lookup_table[index & 0xf] ^ ((crc << 4) & 0xf0);
  }

  return crc;
}

static void prv_wait_for_flash_not_busy(void) {
  while ((*(volatile uint32_t *)FLASH_SR_ADDR) & (1 << 16)); // BSY flag in FLASH_SR
}

__attribute__((__noreturn__)) void Reset_Handler(void) {
  // Disable all interrupts
  __asm__("cpsid i" : : : "memory");

  volatile uint32_t *flash_sr = (volatile uint32_t *)FLASH_SR_ADDR;
  volatile p_size_t *data = (volatile p_size_t *)DATA_ADDR;
  volatile Header *header = (volatile Header *)HEADER_ADDR;
  header->state = STATE_WAITING;

  while(1) {
    switch (header->state) {
      case STATE_WRITE:
        prv_wait_for_flash_not_busy();
        for (uint32_t i = 0; i < header->length / sizeof(p_size_t); i++) {
          header->addr[i] = data[i];
          __asm__("isb 0xF":::"memory");
          __asm__("dsb 0xF":::"memory");
          /// Wait until flash isn't busy
          prv_wait_for_flash_not_busy();
          if (*flash_sr & (0x1f << 4)) {
            // error raised, set bad state
            header->state = *flash_sr;
          }
          if (header->addr[i] != data[i]) {
            header->state = 0xbd;
          }
        }
        header->addr += header->length / sizeof(p_size_t);
        header->state = STATE_WAITING;
        break;
      case STATE_CRC:
        *data = prv_crc8((uint8_t *)header->addr, header->length);
        header->state = STATE_WAITING;
        break;
      default:
        break;
    }
  }

  __builtin_unreachable();
}

//! These symbols are defined in the linker script for use in initializing
//! the data sections. uint8_t since we do arithmetic with section lengths.
//! These are arrays to avoid the need for an & when dealing with linker symbols.
extern uint8_t _estack[];


__attribute__((__section__(".isr_vector"))) const void * const vector_table[] = {
  _estack,
  Reset_Handler
};
