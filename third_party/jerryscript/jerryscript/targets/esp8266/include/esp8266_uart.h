/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 *  Copyright (C) 2014 -2016  Espressif System
 *
 */

#ifndef __ESP8266_UART_H__
#define __ESP8266_UART_H__

typedef enum {
  UART0 = 0x0,
  UART1 = 0x1,
} UART_Port;

typedef enum {
  BIT_RATE_300     = 300,
  BIT_RATE_600     = 600,
  BIT_RATE_1200    = 1200,
  BIT_RATE_2400    = 2400,
  BIT_RATE_4800    = 4800,
  BIT_RATE_9600    = 9600,
  BIT_RATE_19200   = 19200,
  BIT_RATE_38400   = 38400,
  BIT_RATE_57600   = 57600,
  BIT_RATE_74880   = 74880,
  BIT_RATE_115200  = 115200,
  BIT_RATE_230400  = 230400,
  BIT_RATE_460800  = 460800,
  BIT_RATE_921600  = 921600,
  BIT_RATE_1843200 = 1843200,
  BIT_RATE_3686400 = 3686400,
} UART_BautRate;

#endif
