/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef JMEM_CONFIG_H
#define JMEM_CONFIG_H

#include "config.h"

/**
 * Size of heap
 */
#define JMEM_HEAP_SIZE ((size_t) (CONFIG_MEM_HEAP_AREA_SIZE))

/**
 * Logarithm of required alignment for allocated units/blocks
 */
#define JMEM_ALIGNMENT_LOG   3

#endif /* !JMEM_CONFIG_H */
