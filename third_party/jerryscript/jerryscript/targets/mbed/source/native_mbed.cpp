/* SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mbed-drivers/mbed.h"

#include "native_mbed.h"

void native_led (int port, int val)
{
  static const PinName portmap[] = { LED1, LED2, LED3, LED4 };
  static DigitalOut led (portmap[port]);
  led = val;
}
