/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

/*
 *  Copyright (C) 2014 -2016  Espressif System
 *
 */

#include "esp_common.h"

#include "esp8266_gpio.h"


//-----------------------------------------------------------------------------

void gpio_output_conf(uint32 set_mask, uint32 clear_mask, uint32 enable_mask,
                      uint32 disable_mask) {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, set_mask);
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, clear_mask);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, enable_mask);
  GPIO_REG_WRITE(GPIO_ENABLE_W1TC_ADDRESS, disable_mask);
}


uint32 gpio_input_get(void) {
  return GPIO_REG_READ(GPIO_IN_ADDRESS);
}
