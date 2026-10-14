/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <apps/system/timeline/text_node.h>

PBL_WEAK void graphics_text_node_destroy(GTextNode *node) {
}

PBL_WEAK void graphics_text_node_get_size(GTextNode *node, GContext *ctx, const GRect *box,
                                          const GTextNodeDrawConfig *config, GSize *size_out) {
}

PBL_WEAK void graphics_text_node_draw(GTextNode *node, GContext *ctx, const GRect *box,
                                      const GTextNodeDrawConfig *config, GSize *size_out) {
}

PBL_WEAK bool graphics_text_node_container_add_child(GTextNodeContainer *parent, GTextNode *child) {
  return 0;
}

PBL_WEAK GTextNodeText *graphics_text_node_create_text(size_t buffer_size) {
  return nullptr;
}

PBL_WEAK GTextNodeHorizontal *graphics_text_node_create_horizontal(size_t max_nodes) {
  return nullptr;
}

PBL_WEAK GTextNodeCustom *graphics_text_node_create_custom(GTextNodeDrawCallback callback,
                                                           void *user_data) {
  return nullptr;
}
