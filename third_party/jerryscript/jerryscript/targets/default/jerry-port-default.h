/* SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JERRY_PORT_DEFAULT_H
#define JERRY_PORT_DEFAULT_H

#include "jerry-port.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/** \addtogroup jerry_port_default Default Jerry engine port API
 * These functions are only available if the default port of Jerry is used.
 * @{
 */

void jerry_port_default_set_abort_on_fail (bool);
bool jerry_port_default_is_abort_on_fail (void);

jerry_log_level_t jerry_port_default_get_log_level (void);
void jerry_port_default_set_log_level (jerry_log_level_t);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* !JERRY_PORT_DEFAULT_H */
