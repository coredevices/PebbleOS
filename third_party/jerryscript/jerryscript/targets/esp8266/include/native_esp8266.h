/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __NATIVE_ESP8266_H__
#define __NATIVE_ESP8266_H__

#ifdef __cplusplus
extern "C" {
#endif


void native_gpio_dir (int, int);
void native_gpio_set (int, int);
int native_gpio_get (int);


#ifdef __cplusplus
}
#endif

#endif
