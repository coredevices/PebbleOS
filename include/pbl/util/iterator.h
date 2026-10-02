/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

/**
 * @defgroup util_iterator Iterator
 * @ingroup util
 * @brief Generic bidirectional iterator over caller-defined state.
 *
 * Gives iteration a consistent shape and lets unit tests drive it.
 * @{
 */

/** @brief Iterator state, owned by the caller. */
typedef void *IteratorState;

/**
 * @brief Step function of an iterator.
 *
 * @param state Iterator state, updated in place.
 * @return true if the iterator moved.
 */
typedef bool (*IteratorCallback)(IteratorState state);

/** @brief Iterator. */
typedef struct {
  /** Moves to the next element. */
  IteratorCallback next;
  /** Moves to the previous element. */
  IteratorCallback prev;
  /** State passed to the callbacks. */
  IteratorState state;
} Iterator;

/** @brief Iterator without callbacks or state. */
#define ITERATOR_EMPTY ((Iterator){0, 0, 0})

/**
 * @brief Initialize an iterator.
 *
 * @param[out] iter Iterator.
 * @param next Step forward.
 * @param prev Step backward.
 * @param state Iterator state.
 */
void iter_init(Iterator *iter, IteratorCallback next, IteratorCallback prev, IteratorState state);

/**
 * @brief Move to the next element.
 *
 * @param iter Iterator, with a @c next callback.
 * @return true if the iterator moved.
 */
bool iter_next(Iterator *iter);

/**
 * @brief Move to the previous element.
 *
 * @param iter Iterator, with a @c prev callback.
 * @return true if the iterator moved.
 */
bool iter_prev(Iterator *iter);

/** @} */
