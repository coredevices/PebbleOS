/* Copyright 2014-2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

