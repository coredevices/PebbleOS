/* SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __JERRY_RUN_H__
#define __JERRY_RUN_H__

#ifdef __cplusplus
extern "C" {
#endif


int js_entry (const char *source_p, const size_t source_size);
int js_eval (const char *source_p, const size_t source_size);
int js_loop (uint32_t ticknow);
void js_exit (void);


#ifdef __cplusplus
}
#endif

#endif
