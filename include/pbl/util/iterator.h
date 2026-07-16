/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Simple utility for enforcing consistent use of the iterator pattern
//! and facilitate unit testing.
#pragma once

#include <stdbool.h>

typedef void* IteratorState;

typedef bool (*IteratorCallback)(IteratorState state);

typedef struct {
  IteratorCallback next;
  IteratorCallback prev;
  IteratorState state;
} Iterator;

#define ITERATOR_EMPTY ((Iterator){ 0, 0, 0 })

void iter_init(Iterator* iter, IteratorCallback next, IteratorCallback prev, IteratorState state);

//! @return true if successfully moved to next node
bool iter_next(Iterator* iter);

//! @return true if successfully moved to previous node
bool iter_prev(Iterator* iter);

IteratorState iter_get_state(Iterator* iter);

