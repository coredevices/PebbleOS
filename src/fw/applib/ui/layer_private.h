/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "layer.h"

Layer *__layer_tree_traverse_next__test_accessor(Layer *stack[],
    int const max_depth, uint8_t *current_depth, const bool descend);

typedef bool (*LayerIteratorFunc)(Layer *layer, void *ctx);

void layer_process_tree(Layer *node, void *ctx, LayerIteratorFunc iterator_func);
