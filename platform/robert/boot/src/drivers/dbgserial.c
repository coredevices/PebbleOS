/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dbgserial.h"

#include "drivers/gpio.h"
#include "drivers/periph_config.h"
#include "stm32f7haxx_rcc.h"
#include "stm32f7haxx_gpio.h"
#include "util/attributes.h"
#include "util/cobs.h"
#include "util/crc32.h"
#include "util/net.h"
#include "util/misc.h"

#include <stdint.h>
#include <string.h>

#define MAX_MESSAGE (256)
#define FRAME_DELIMITER '\x55'
#define PULSE_TRANSPORT_PUSH (0x5021)
#define PULSE_PROTOCOL_LOGGING (0x0003)

static const int SERIAL_BAUD_RATE = 1000000;
static USART_TypeDef *const DBGSERIAL_UART = USART3;

typedef struct PACKED PulseFrame {
  net16 protocol;
  unsigned char information[];
} PulseFrame;

typedef struct PACKED PushPacket {
  net16 protocol;
  net16 length;
  unsigned char information[];
} PushPacket;

static const unsigned char s_message_header[] = {
  // Message type: text
  1,
  // Source filename
  'B', 'O', 'O', 'T', 'L', 'O', 'A', 'D', 'E', 'R', 0, 0, 0, 0, 0, 0,
  // Log level and task
  '*', '*',
  // Timestamp
  0, 0, 0, 0, 0, 0, 0, 0,
  // Line number
  0, 0,
};

static size_t s_message_length = 0;
static unsigned char s_message_buffer[MAX_MESSAGE];

void dbgserial_init(void) {
  // Enable GPIO and UART3 peripheral clocks
  periph_config_enable(GPIOD, RCC_AHB1Periph_GPIOD);
  periph_config_enable(DBGSERIAL_UART, RCC_APB1Periph_USART3);

  DBGSERIAL_UART->CR1 &= ~USART_CR1_UE;

  AfConfig tx_cfg = {
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_8,
    .gpio_pin_source = GPIO_PinSource8,
    .gpio_af = GPIO_AF7_USART3
  };

  gpio_af_init(&tx_cfg, GPIO_OType_PP, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);

  AfConfig rx_cfg = {
    .gpio = GPIOD,
    .gpio_pin = GPIO_Pin_9,
    .gpio_pin_source = GPIO_PinSource9,
    .gpio_af = GPIO_AF7_USART3
  };

  gpio_af_init(&rx_cfg, GPIO_OType_PP, GPIO_Speed_50MHz, GPIO_PuPd_NOPULL);

  // configure the UART peripheral control registers and baud rate
  // - 8-bit word length
  // - no parity
  // - RX / TX enabled
  // - 1 stop bit
  // - no flow control

  const int k_div_precision = 100;

  RCC_ClocksTypeDef clocks;
  RCC_GetClocksFreq(&clocks);

  // calculate the baud rate value
  const uint64_t scaled_apbclock = k_div_precision * clocks.PCLK1_Frequency;
  const uint32_t div = (scaled_apbclock / SERIAL_BAUD_RATE);
  const uint32_t brr = (div & 0xFFF0) | ((div & 0xF) >> 1);

  DBGSERIAL_UART->BRR = brr / k_div_precision;
  DBGSERIAL_UART->CR2 = 0;
  DBGSERIAL_UART->CR3 = 0;
  DBGSERIAL_UART->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_UE;
}

static void prv_putchar(uint8_t c) {
  while ((DBGSERIAL_UART->ISR & USART_ISR_TXE) == 0) continue;
  DBGSERIAL_UART->TDR = c;
  while ((DBGSERIAL_UART->ISR & USART_ISR_TXE) == 0) continue;
}

void dbgserial_print(const char* str) {
  for (; *str && s_message_length < MAX_MESSAGE; ++str) {
    if (*str == '\n') {
      dbgserial_newline();
    } else if (*str != '\r') {
      s_message_buffer[s_message_length++] = *str;
    }
  }
}

void dbgserial_newline(void) {
  uint32_t crc;
  size_t raw_length = sizeof(PulseFrame) + sizeof(PushPacket) +
                      sizeof(s_message_header) + s_message_length + sizeof(crc);
  unsigned char raw_packet[raw_length];

  PulseFrame *frame = (PulseFrame *)raw_packet;
  frame->protocol = hton16(PULSE_TRANSPORT_PUSH);

  PushPacket *transport = (PushPacket *)frame->information;
  transport->protocol = hton16(PULSE_PROTOCOL_LOGGING);
  transport->length = hton16(sizeof(PushPacket) + sizeof(s_message_header) +
                             s_message_length);

  unsigned char *app = transport->information;
  memcpy(app, s_message_header, sizeof(s_message_header));
  memcpy(&app[sizeof(s_message_header)], s_message_buffer,
         s_message_length);

  crc = crc32(CRC32_INIT, raw_packet, raw_length - sizeof(crc));
  memcpy(&raw_packet[raw_length - sizeof(crc)], &crc, sizeof(crc));

  unsigned char cooked_packet[MAX_SIZE_AFTER_COBS_ENCODING(raw_length)];
  size_t cooked_length = cobs_encode(cooked_packet, raw_packet, raw_length);

  prv_putchar(FRAME_DELIMITER);
  for (size_t i = 0; i < cooked_length; ++i) {
    if (cooked_packet[i] == FRAME_DELIMITER) {
      prv_putchar('\0');
    } else {
      prv_putchar(cooked_packet[i]);
    }
  }
  prv_putchar(FRAME_DELIMITER);

  s_message_length = 0;
}

void dbgserial_putstr(const char* str) {
  dbgserial_print(str);

  dbgserial_newline();
}

void dbgserial_print_hex(uint32_t value) {
  char buf[12];
  itoa_hex(value, buf, sizeof(buf));
  dbgserial_print(buf);
}
