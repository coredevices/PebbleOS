/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Performs initialization for the current FreeRTOS task. Currently, all it does is it
//! attempts to seed the current task's REENT random seed with a value from the
//! hardware random number generator.
void task_init(void);
