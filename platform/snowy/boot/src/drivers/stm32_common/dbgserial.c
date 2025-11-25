/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/dbgserial.h"

#include "drivers/periph_config.h"
#include "system/passert.h"

#include "drivers/gpio.h"

#if defined(MICRO_FAMILY_STM32F2)
#include "stm32f2xx_rcc.h"
#include "stm32f2xx_gpio.h"
#include "stm32f2xx_usart.h"
#elif defined(MICRO_FAMILY_STM32F4)
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_usart.h"
#endif
#include "util/attributes.h"
#include "util/cobs.h"
#include "util/crc32.h"
#include "util/net.h"
#include "util/misc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_MESSAGE (256)
#define FRAME_DELIMITER '\x55'
#define PULSE_TRANSPORT_PUSH (0x5021)
#define PULSE_PROTOCOL_LOGGING (0x0003)

static bool s_initialized;

static const int SERIAL_BAUD_RATE = 1000000;

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
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;

  periph_config_acquire_lock();

  /* Enable GPIO and UART3 peripheral clocks */
  gpio_use(GPIOC);
  periph_config_enable(RCC_APB1PeriphClockCmd, RCC_APB1Periph_USART3);

  //USART_OverSampling8Cmd(USART3, ENABLE);

  /* Connect PXx to USARTx_Tx*/
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);

  /* Connect PXx to USARTx_Rx*/
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);

  /* Configure USART Tx as alternate function  */
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  /* Configure USART Rx as alternate function  */
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  /* USART configuration */
  USART_InitStructure.USART_BaudRate = SERIAL_BAUD_RATE;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART3, &USART_InitStructure);

  /* Enable USART */
  USART_Cmd(USART3, ENABLE);

  periph_config_release_lock();
  gpio_release(GPIOC);

  s_initialized = true;
}

static void prv_putchar(uint8_t c) {
  if (!s_initialized) {
    return;
  }

  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET) continue;
  USART_SendData(USART3, c);
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET) continue;
}

static void prv_flush(void) {
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
  if (!s_initialized) {
    return;
  }

  dbgserial_print(str);
  prv_flush();
}

void dbgserial_print(const char* str) {
  if (!s_initialized) {
    return;
  }

  for (; *str && s_message_length < MAX_MESSAGE; ++str) {
    if (*str == '\n') {
      prv_flush();
    } else if (*str != '\r') {
      s_message_buffer[s_message_length++] = *str;
    }
  }
}

void dbgserial_print_hex(uint32_t value) {
  char buf[12];
  itoa(value, buf, sizeof(buf));
  dbgserial_print(buf);
}

