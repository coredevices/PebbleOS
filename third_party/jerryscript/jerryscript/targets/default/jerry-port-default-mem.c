/* SPDX-FileCopyrightText: 2014-2016 Samsung Electronics Co., Ltd. */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdarg.h>

#include "jerry-port.h"

#define JMEM_HEAP_INTERNAL

#ifndef JERRY_HEAP_SECTION_ATTR
static jmem_heap_t jmem_heap;
#else
static jmem_heap_t jmem_heap __attribute__ ((section (JERRY_HEAP_SECTION_ATTR)));
#endif

jmem_heap_t *jerry_port_init_heap(void) {
  memset(&jmem_heap, 0, sizeof(jmem_heap));
  return &jmem_heap;
}

void jerry_port_finalize_heap(jmem_heap_t *jmem_heap) {
  return;
}

jmem_heap_t *jerry_port_get_heap(void) {
  return &jmem_heap;
}

