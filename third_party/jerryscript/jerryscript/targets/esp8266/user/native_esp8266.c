/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
*******************************************************************************/

#include "esp_common.h"

#include "user_config.h"
#include "esp8266_gpio.h"


void native_gpio_dir(int port, int value) {
  if (value) {
    GPIO_AS_OUTPUT(1 << port);
  }
  else {
    GPIO_AS_INPUT(1 << port);
  }
}


void native_gpio_set(int port, int value) {
  GPIO_OUTPUT_SET(port, value);
}


int native_gpio_get(int port) {
  return GPIO_INPUT_GET(port);
}
