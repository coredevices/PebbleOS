/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */

#ifndef _DT_BINDINGS_INPUT_INPUT_H
#define _DT_BINDINGS_INPUT_INPUT_H

/* Key codes for linux,code, with the values of the Linux input event codes */
#define KEY_UP     103
#define KEY_RIGHT  106
#define KEY_DOWN   108
#define KEY_WAKEUP 143
#define KEY_BACK   158
#define KEY_SELECT 0x161

/* Relative axes for linux,axis, with the values of the Linux input event codes */
#define REL_X     0x00
#define REL_Y     0x01
#define REL_WHEEL 0x08

#endif /* _DT_BINDINGS_INPUT_INPUT_H */
