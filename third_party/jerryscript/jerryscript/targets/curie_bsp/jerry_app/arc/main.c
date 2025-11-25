/* SPDX-FileCopyrightText: 2016 Intel Corporation */
/* SPDX-License-Identifier: Apache-2.0 */
/* infra */
#include "infra/log.h"
#include "infra/bsp.h"
#include "infra/xloop.h"
#include "cfw/cfw.h"

static xloop_t loop;

void main (void)
{
  T_QUEUE queue = bsp_init ();

  pr_info (LOG_MODULE_MAIN, "BSP init done");

  cfw_init (queue);
  pr_info (LOG_MODULE_MAIN, "CFW init done");

  xloop_init_from_queue (&loop, queue);

  xloop_run (&loop);
}
