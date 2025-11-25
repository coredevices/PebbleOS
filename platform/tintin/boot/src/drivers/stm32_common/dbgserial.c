/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/dbgserial.h"

#include "drivers/periph_config.h"

#include "drivers/gpio.h"

#include "stm32f2xx_rcc.h"
#include "stm32f2xx_gpio.h"
#include "stm32f2xx_usart.h"
#include "misc.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static const int SERIAL_BAUD_RATE = 230400;

void dbgserial_init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;

  // Enable GPIO and UART3 peripheral clocks
  gpio_use(GPIOC);
  periph_config_enable(RCC_APB1PeriphClockCmd, RCC_APB1Periph_USART3);

  // USART_OverSampling8Cmd(USART3, ENABLE);

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

  gpio_release(GPIOC);
}

static void prv_putchar(uint8_t c) {
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET) continue;
  USART_SendData(USART3, c);
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET) continue;
}

void dbgserial_print(const char* str) {
  while (*str) {
    prv_putchar(*str);
    ++str;
  }
}

void dbgserial_newline(void) {
  prv_putchar('\r');
  prv_putchar('\n');
}

void dbgserial_putstr(const char* str) {
  dbgserial_print(str);

  dbgserial_newline();
}

void dbgserial_print_hex(uint32_t value) {
  char buf[12];
  itoa(value, buf, sizeof(buf));
  dbgserial_print(buf);
}
