/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/iterator.h"

#include "util/assert.h"

#include <stdbool.h>

void iter_init(Iterator* iter, IteratorCallback next, IteratorCallback prev, IteratorState state) {
  *iter = (Iterator) {
    .next = next,
    .prev = prev,
    .state = state
  };
}

bool iter_next(Iterator* iter) {
  UTIL_ASSERT(iter->next);
  return iter->next(iter->state);
}

bool iter_prev(Iterator* iter) {
  UTIL_ASSERT(iter->prev);
  return iter->prev(iter->state);
}

