/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __JERRY_EXTAPI_H__
#define __JERRY_EXTAPI_H__

#define JERRY_STANDALONE_EXIT_CODE_OK   (0)
#define JERRY_STANDALONE_EXIT_CODE_FAIL (1)


#ifdef __cplusplus
extern "C" {
#endif


void js_register_functions (void);


#ifdef __cplusplus
}
#endif

#endif
