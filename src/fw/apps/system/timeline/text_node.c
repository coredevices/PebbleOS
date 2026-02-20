/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "text_node.h"

#include "kernel/pbl_malloc.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "util/math.h"

#include "applib/graphics/graphics.h"

//! The max text node traversal draw depth.
//! The deepest layout as of this commit is 4, belonging to the calendar layout glance time.
//! If a greater depth is necessary, this number can be increased provided that there is enough heap
//! space. Each depth uses sizeof(GTextNodeDrawContext) heap memory in bytes.
#define MAX_DRAW_DEPTH 8

#define ToGValue(value) ((GValue *)value)

typedef int16_t GValue;

_Static_assert(sizeof(GPoint) == sizeof(GValue[2]),
               "TextNode requires a GPoint to be equivalent to a GValue[2]");
_Static_assert(sizeof(GSize) == sizeof(GValue[2]),
               "TextNode requires a GSize to be equivalent to a GValue[2]");

typedef enum {
  GAxis_X = 0,
  GAxis_Y,
  GAxis_W = GAxis_X,
  GAxis_H = GAxis_Y,
} GAxis;

typedef enum {
  GAxisAlign_Min = 0,
  GAxisAlign_Center,
  GAxisAlign_Max,
} GAxisAlign;

_Static_assert((((int)GAxisAlign_Min    == GTextAlignmentLeft) &&
                ((int)GAxisAlign_Center == GTextAlignmentCenter) &&
                ((int)GAxisAlign_Max    == GTextAlignmentRight)),
               "TextNode requires GTextAlignment == the ordered set (0, 1, 2), left to right");
_Static_assert((((int)GAxisAlign_Min    == GVerticalAlignmentTop) &&
                ((int)GAxisAlign_Center == GVerticalAlignmentCenter) &&
                ((int)GAxisAlign_Max    == GVerticalAlignmentBottom)),
               "TextNode requires GVerticalAlignment == the ordered set (0, 1, 2), top to bottom");

typedef struct {
  const GTextNodeDrawConfig *config; //!< Draw configuration passed by the user
  GTextNode *node; //!< GTextNode the context belongs to
  GContext *gcontext; //!< Graphics context to draw with
  const GRect *draw_box; //!< Drawing box in local coordinates passed by the user
  GSize *size_out; //!< GSize pointer to write the calculated size to
  //! GRect representing the drawing cursor. For leaf nodes, this is simply the draw_box offset by
  //! the node's offset. For containers, this is passed as the drawing box to its children, and
  //! shrinks along the container's axis after drawing each child.
  GRect box;
  //! GRect of the clip box saved before apply a text node's draw box as its clip box. Only used
  //! if the text node specifically requested clipping.
  GRect cached_clip_box;
  //! GSize representing the size of the container. Used by containers only. This size starts at
  //! GSizeZero and grows after drawing each child.
  GSize size;
  bool render; //!< true if this context should render, otherwise false only calculating size
  //! true if this context should neither render nor calculate size, otherwise false. size_out will
  //! instead be derived from the node's cached size, and the render box will similarly be advanced
  //! by a size derived from the node's cached size.
  bool cached;
} GTextNodeDrawContext;

typedef bool (*ContainerIterCallback)(GTextNode *node, void *context);

typedef void (*GTextNodeDestroyMethod)(GTextNode *node);

typedef void (*GTextNodeDrawMethod)(GTextNodeDrawContext *context);

typedef bool (*GTextNodeAddChildMethod)(GTextNodeContainer *parent, GTextNode *child);

typedef bool (*GTextNodeDrawChildMethod)(GTextNodeDrawContext *context, GTextNode *child_node);

typedef GAxisAlign (*GTextNodeGetAxisAlignMethod)(GTextNode *node);

typedef struct {
  GTextNodeDestroyMethod destructor;
  GTextNodeDrawMethod draw;
  GTextNodeDrawMethod will_draw;
  GTextNodeDrawMethod did_draw;
  bool is_container;
} GTextNodeBaseImpl;

typedef struct {
  GTextNodeBaseImpl base;
  GTextNodeGetAxisAlignMethod get_axis_align;
  GTextNodeAddChildMethod add_child;
  GTextNodeDrawChildMethod will_draw_child;
  GTextNodeDrawChildMethod did_draw_child;
  GAxis axis;
} GTextNodeContainerImpl;

static const GTextNodeContainerImpl *prv_container(GTextNode *node);

static void prv_draw_text_node(GTextNode *node, GContext *ctx, const GRect *box,
                               const GTextNodeDrawConfig *config, bool render, GSize *size_out);

static bool prv_container_each(GTextNodeContainer *container_node, ContainerIterCallback callback,
                               void *context) {
  const size_t num_nodes = container_node->num_nodes;
  for (size_t i = 0; i < num_nodes; i++) {
    GTextNode *node = container_node->nodes[i];
    if (callback(node, context) == false) {
      return false;
    }
  }
  return true;
}

static GAxis prv_get_opposite_axis(GAxis axis) {
  switch (axis) {
    case GAxis_X:
      return GAxis_Y;
    case GAxis_Y:
      return GAxis_X;
  }
  return GAxis_X;
}

GTextNodeText *graphics_text_node_create_text(size_t buffer_size) {
  GTextNodeText *text_node = task_zalloc(sizeof(GTextNodeText) + buffer_size);
  if (text_node) {
    *text_node = (GTextNodeText) {
      .node.type = GTextNodeType_Text,
      .node.free_on_destroy = true,
      .text = buffer_size ? (char *)(text_node + 1) : NULL,
    };
  }
  return text_node;
}

GTextNodeTextDynamic *graphics_text_node_create_text_dynamic(
    size_t buffer_size, GTextNodeTextDynamicUpdate update, void *user_data) {
  GTextNodeTextDynamic *text_node = task_zalloc(sizeof(GTextNodeTextDynamic) + buffer_size);
  if (text_node) {
    *text_node = (GTextNodeTextDynamic) {
      .text.node.type = GTextNodeType_TextDynamic,
      .text.node.free_on_destroy = true,
      .text.text = buffer_size ? (char *)text_node->buffer : NULL,
      .update = update,
      .user_data = user_data,
      .buffer_size = buffer_size,
    };
  }
  return text_node;
}

static GTextNodeContainer *prv_create_container(GTextNodeType type, size_t size, size_t max_nodes) {
  GTextNodeContainer *container_node = task_zalloc(size + max_nodes * sizeof(GTextNode *));
  if (container_node) {
    *container_node = (GTextNodeContainer) {
      .node.type = type,
      .node.free_on_destroy = true,
      .max_nodes = max_nodes,
      .nodes = max_nodes ? (GTextNode **)((uint8_t *)container_node + size) : NULL,
    };
  }
  return container_node;
}

GTextNodeHorizontal *graphics_text_node_create_horizontal(size_t max_nodes) {
  return (GTextNodeHorizontal *)prv_create_container(GTextNodeType_Horizontal,
                                                     sizeof(GTextNodeHorizontal), max_nodes);
}

GTextNodeVertical *graphics_text_node_create_vertical(size_t max_nodes) {
  return (GTextNodeVertical *)prv_create_container(GTextNodeType_Vertical,
                                                   sizeof(GTextNodeVertical), max_nodes);
}

GTextNodeCustom *graphics_text_node_create_custom(GTextNodeDrawCallback callback,
                                                  void *user_data) {
  GTextNodeCustom *custom_node = task_malloc(sizeof(GTextNodeCustom));
  if (custom_node) {
    *custom_node = (GTextNodeCustom) {
      .node.type = GTextNodeType_Custom,
      .node.free_on_destroy = true,
      .callback = callback,
      .user_data = user_data,
    };
  }
  return custom_node;
}

static void prv_destroy_text_node_base(GTextNode *node) {
  if (node && node->free_on_destroy) {
    task_free(node);
  }
}

static bool prv_destroy_container_iter(GTextNode *node, void *context) {
  graphics_text_node_destroy(node);
  return true;
}

static void prv_destroy_text_node_container(GTextNode *node) {
  if (!node) {
    return;
  }
  GTextNodeContainer *container_node = (GTextNodeContainer *)node;
  prv_container_each(container_node, prv_destroy_container_iter, NULL);
  prv_destroy_text_node_base(&container_node->node);
}

static bool prv_container_add_child(GTextNodeContainer *container_node, GTextNode *child) {
  if (!container_node || !child) {
    return false;
  }
  const size_t num_nodes = container_node->num_nodes;
  if (num_nodes >= container_node->max_nodes) {
    return false;
  }
  GTextNode *prev_child = (num_nodes > 0) ? container_node->nodes[num_nodes - 1] : NULL;
  container_node->nodes[container_node->num_nodes++] = child;
  if (prev_child) {
    prev_child->sibling = child;
  }
  return true;
}

static void prv_set_size(GSize *size, const GSize *exact_size) {
  if (exact_size->w) {
    size->w = exact_size->w;
  }
  if (exact_size->h) {
    size->h = exact_size->h;
  }
}

static void prv_clip_width(GSize *size, const GSize *max_size) {
  if (max_size->w && size->w > max_size->w) {
    size->w = max_size->w;
  }
}

static void prv_clip_height(GSize *size, const GSize *max_size) {
  if (max_size->h && size->h > max_size->h) {
    size->h = max_size->h;
  }
}

static void prv_clip_size(GSize *size, const GSize *max_size) {
  prv_clip_width(size, max_size);
  prv_clip_height(size, max_size);
}

static void prv_draw_text_node_text(GTextNodeDrawContext *ctx) {
  GTextNodeText *text_node = (GTextNodeText *)ctx->node;
  prv_clip_size(&ctx->box.size, &text_node->max_size);
  TextLayoutExtended layout = {
    .line_spacing_delta = text_node->line_spacing_delta,
  };
  const GTextNodeDrawConfig *config = ctx->config;
  if (config) {
    if (config->text_flow) {
      graphics_text_attributes_enable_screen_text_flow((TextLayout *)&layout,
                                                       config->content_inset);
    }
    if (config->paging) {
      graphics_text_attributes_enable_paging((TextLayout *)&layout,
                                             gpoint_add(*config->origin_on_screen, ctx->box.origin),
                                             *config->page_frame);
    }
  }
  if (ctx->render) {
    const GColor prev_text_color = ctx->gcontext->draw_state.text_color;
    if (!gcolor_is_invisible(text_node->color)) {
      ctx->gcontext->draw_state.text_color = text_node->color;
    }
    graphics_draw_text(ctx->gcontext, text_node->text, text_node->font, ctx->box,
                       text_node->overflow, text_node->alignment, (TextLayout *)&layout);
    ctx->gcontext->draw_state.text_color = prev_text_color;
  } else {
    graphics_text_layout_get_max_used_size(ctx->gcontext, text_node->text, text_node->font,
                                           ctx->box, text_node->overflow, text_node->alignment,
                                           (TextLayout *)&layout);
  }
  if (ctx->size_out) {
    *ctx->size_out = layout.max_used_size;
  }
}

static void prv_draw_text_node_text_dynamic(GTextNodeDrawContext *ctx) {
  GTextNodeTextDynamic *text_node = (GTextNodeTextDynamic *)ctx->node;
  text_node->update(ctx->gcontext, &text_node->text.node, &ctx->box, ctx->config, ctx->render,
                    (char *)text_node->text.text, text_node->buffer_size, text_node->user_data);
  prv_draw_text_node_text(ctx);
}

static GAxisAlign prv_get_axis_align_horizontal(GTextNode *node) {
  return (GAxisAlign)((GTextNodeHorizontal *)node)->horizontal_alignment;
}

static GAxisAlign prv_get_axis_align_vertical(GTextNode *node) {
  return (GAxisAlign)((GTextNodeVertical *)node)->vertical_alignment;
}

static void prv_align_axis(GTextNodeDrawContext *ctx) {
  GSize max_used_size;
  const GAxis axis = prv_container(ctx->node)->axis;
  const GAxisAlign align = prv_container(ctx->node)->get_axis_align(ctx->node);
  graphics_text_node_get_size(ctx->node, ctx->gcontext, ctx->draw_box, ctx->config, &max_used_size);
  const int16_t excess_length = ToGValue(&ctx->box.size)[axis] - ToGValue(&max_used_size)[axis];
  if (align == GAxisAlign_Center) {
    ToGValue(&ctx->box.origin)[axis] += excess_length / 2;
  } else if (align == GAxisAlign_Max) {
    ToGValue(&ctx->box.origin)[axis] += excess_length;
  }
}

static void prv_will_draw_container(GTextNodeDrawContext *ctx) {
  GTextNodeContainer *container = (GTextNodeContainer *)ctx->node;
  prv_set_size(&ctx->box.size, &container->size);
  if (ctx->render) {
    prv_align_axis(ctx);
  }
}

static void prv_did_draw_container(GTextNodeDrawContext *ctx) {
  GTextNodeContainer *container = (GTextNodeContainer *)ctx->node;
  if (!ctx->cached) {
    prv_set_size(&ctx->size, &container->size);
    *ctx->size_out = ctx->size;
  }
}

static bool prv_will_draw_container_child(GTextNodeDrawContext *ctx, GTextNode *child_node) {
  GTextNode *parent_node = ctx->node;
  parent_node->cached_size = ctx->box.size;
  if (parent_node->type == GTextNodeType_Horizontal &&
      child_node->type == GTextNodeType_Vertical) {
    prv_clip_width(&ctx->box.size, &child_node->cached_size);
  } else if (parent_node->type == GTextNodeType_Vertical &&
             child_node->type == GTextNodeType_Horizontal) {
    prv_clip_height(&ctx->box.size, &child_node->cached_size);
  }
  return true;
}

static bool prv_did_draw_container_child(GTextNodeDrawContext *ctx, GTextNode *child_node) {
  const GValue *size = ToGValue(&child_node->cached_size);
  const GAxis axis = prv_container(ctx->node)->axis;
  const GAxis max_axis = prv_get_opposite_axis(axis);
  ToGValue(&ctx->size)[axis] += size[axis];
  ToGValue(&ctx->size)[max_axis] = MAX(ToGValue(&ctx->size)[max_axis], size[max_axis]);
  ctx->box.size = ctx->node->cached_size;
  ToGValue(&ctx->box.origin)[axis] += size[axis];
  ToGValue(&ctx->box.size)[axis] -= size[axis];
  return true;
}

static void prv_draw_text_node_custom(GTextNodeDrawContext *ctx) {
  GTextNodeCustom *custom_node = (GTextNodeCustom *)ctx->node;
  custom_node->callback(ctx->gcontext, &ctx->box, ctx->config, ctx->render, ctx->size_out,
                        custom_node->user_data);
}

static void prv_draw_noop(GTextNodeDrawContext *ctx) { }

static const GTextNodeBaseImpl s_text_impl = {
  .destructor = prv_destroy_text_node_base,
  .will_draw = prv_draw_noop,
  .did_draw = prv_draw_noop,
  .draw = prv_draw_text_node_text,
};

static const GTextNodeBaseImpl s_text_dynamic_impl = {
  .destructor = prv_destroy_text_node_base,
  .will_draw = prv_draw_noop,
  .did_draw = prv_draw_noop,
  .draw = prv_draw_text_node_text_dynamic,
};

static const GTextNodeBaseImpl s_custom_impl = {
  .destructor = prv_destroy_text_node_base,
  .will_draw = prv_draw_noop,
  .did_draw = prv_draw_noop,
  .draw = prv_draw_text_node_custom,
};

static const GTextNodeContainerImpl s_horizontal_impl = {
  .base.is_container = true,
  .base.destructor = prv_destroy_text_node_container,
  .base.draw = prv_draw_noop,
  .base.will_draw = prv_will_draw_container,
  .base.did_draw = prv_did_draw_container,
  .axis = GAxis_X,
  .get_axis_align = prv_get_axis_align_horizontal,
  .add_child = prv_container_add_child,
  .will_draw_child = prv_will_draw_container_child,
  .did_draw_child = prv_did_draw_container_child,
};

static const GTextNodeContainerImpl s_vertical_impl = {
  .base.is_container = true,
  .base.destructor = prv_destroy_text_node_container,
  .base.draw = prv_draw_noop,
  .base.will_draw = prv_will_draw_container,
  .base.did_draw = prv_did_draw_container,
  .axis = GAxis_Y,
  .get_axis_align = prv_get_axis_align_vertical,
  .add_child = prv_container_add_child,
  .will_draw_child = prv_will_draw_container_child,
  .did_draw_child = prv_did_draw_container_child,
};

static const GTextNodeBaseImpl *s_impl_table[GTextNodeTypeCount] = {
  [GTextNodeType_Text] = &s_text_impl,
  [GTextNodeType_TextDynamic] = &s_text_dynamic_impl,
  [GTextNodeType_Horizontal] = &s_horizontal_impl.base,
  [GTextNodeType_Vertical] = &s_vertical_impl.base,
  [GTextNodeType_Custom] = &s_custom_impl,
};

static const GTextNodeBaseImpl *prv_base(GTextNode *node) {
  return s_impl_table[node->type];
}

static const GTextNodeContainerImpl *prv_container(GTextNode *node) {
  return (const GTextNodeContainerImpl *)s_impl_table[node->type];
}

void graphics_text_node_destroy(GTextNode *node) {
  if (node) {
    prv_base(node)->destructor(node);
  }
}

bool graphics_text_node_container_add_child(GTextNodeContainer *parent, GTextNode *child) {
  if (!parent) {
    return false;
  }
  return prv_container(&parent->node)->add_child(parent, child);
}

static void NOINLINE prv_init_draw_context(GTextNodeDrawContext *ctx, GTextNode *node,
                                           GContext *gcontext, const GRect *box,
                                           const GTextNodeDrawConfig *config, bool render) {
  *ctx = (GTextNodeDrawContext) {
    .config = config,
    .gcontext = gcontext,
    .node = node,
    .draw_box = box,
    .box = { gpoint_add(box->origin, node->offset), box->size },
    .render = render,
    .size_out = &node->cached_size,
  };
}

static bool prv_should_draw_text_node(GTextNode *node, GContext *ctx, const GRect *box,
                                      bool render) {
  if (!render && node->cached_size.h) {
    return false;
  }
  if (render && node->cached_size.h) {
    GRect global_box = grect_to_global_coordinates((GRect) { box->origin, node->cached_size }, ctx);
    grect_clip(&global_box, &ctx->draw_state.clip_box);
    if (!global_box.size.h) {
      return false;
    }
  }
  return true;
}

static void prv_iter_will_draw(GTextNodeDrawContext *ctx, GTextNodeDrawContext *parent_ctx) {
  GTextNode *node = ctx->node;
  if (parent_ctx) {
    prv_container(parent_ctx->node)->will_draw_child(parent_ctx, node);
    prv_init_draw_context(ctx, node, parent_ctx->gcontext, &parent_ctx->box, parent_ctx->config,
                          parent_ctx->render);
  }

  gsize_sub_eq(&ctx->box.size, node->margin);
  prv_base(node)->will_draw(ctx);

  ctx->cached = !prv_should_draw_text_node(node, ctx->gcontext, &ctx->box, ctx->render);
  if (!ctx->cached && node->clip) {
    ctx->cached_clip_box = ctx->gcontext->draw_state.clip_box;
    const GRect draw_box = {
      .origin = gpoint_add(ctx->draw_box->origin, ctx->gcontext->draw_state.drawing_box.origin),
      .size = ctx->draw_box->size,
    };
    grect_clip(&ctx->gcontext->draw_state.clip_box, &draw_box);
  }
}

static void prv_iter_did_draw(GTextNodeDrawContext *ctx, GTextNodeDrawContext *parent_ctx) {
  GTextNode *node = ctx->node;
  prv_base(node)->did_draw(ctx);

  if (!ctx->cached) {
    if (node->clip) {
      ctx->gcontext->draw_state.clip_box = ctx->cached_clip_box;
    }
    gsize_add_eq(ctx->size_out, node->margin);
  }

  if (parent_ctx) {
    prv_container(parent_ctx->node)->did_draw_child(parent_ctx, node);
  }
}

static void prv_draw_text_node_tree(GTextNode *root_node, GContext *gcontext, const GRect *box,
                                    const GTextNodeDrawConfig *config, bool render,
                                    GSize *size_out) {
  // This function implements a depth-first traversal algorithm to draw a TextNode hierarchy using
  // the least amount of stack space as possible by offloading the draw context for each depth level
  // onto the heap.
  //
  // Container TextNodes have extra calculation that are necessary before and after they draw
  // themselves, therefore before a child is drawn, its parent must invoke its `will_draw` method,
  // and `did_draw` after the final child is drawn. This is necessary to setup the draw box that
  // will be passed down to its children as well as to finalize the calculation of the container's
  // size. Similarly, containers also have extra calculation before and after they draw a child in
  // order to shrink the draw box that the next child will receive accordingly. These calculations
  // are both encapsulated in `prv_iter_will_draw` and `prv_iter_did_draw`.
  //
  // In order to visualize this algorithm, imagine these lines below overlaid on the major control
  // statements below:
  //
  //  for      ___ Ascent loop caused by the "if container". With nested containers, the algorithm
  //    \ \  /     will keep ascending by branching into the if and hitting the continue.
  //      \ \.
  //     |  \ \ (if container)
  //     |
  //     |   - --- The body of the main for loop. This section of the loop is exercised with
  //    for        containers that have long stretches of children.
  //        / /
  //      / /
  //    / /  \ ___ Descent loop caused by the nested for loop. With nested containers, the algorithm
  //               can descend multiple times if a container is the last child in a container.
  GTextNodeDrawContext *contexts = task_zalloc_check(sizeof(GTextNodeDrawContext) * MAX_DRAW_DEPTH);
  // Draw contexts are initialized by the parent container. Since this is the first level, level 0,
  // it must be initialized on its own.
  prv_init_draw_context(&contexts[0], root_node, gcontext, box, config, render);
  for (int level = 0; level >= 0;) {
    PBL_ASSERTN(level < MAX_DRAW_DEPTH);
    GTextNodeDrawContext *ctx = &contexts[level];
    GTextNodeDrawContext *parent_ctx = (level > 0) ? &contexts[level - 1] : NULL;

    // We have entered here either by ascending (the continue in the ascent branch below) or by
    // moving on to the next sibling (the break in the descent loop), which may have included
    // descending nodes. The current node will be drawn. Drawing children nodes in a container is
    // considered as part of the container's drawing.
    prv_iter_will_draw(ctx, parent_ctx);

    // If the node is considered cached (we know its size, or in the case of render == true, we know
    // its size && it is clipped off-screen), then we do not draw it, container or otherwise.
    if (!ctx->cached) {
      // Containers do not inherently have their own drawing procedure beyond managing its own size
      // and the shrinking draw box size that it hands over to its children. Therefore if it it is a
      // container, we "draw" it by ascending into its children continuing to the start of this loop
      // (saving stack space), otherwise we call its draw method if it's not a container.
      if (prv_base(ctx->node)->is_container) {
        // This is a container, ascend a level into its first child if it has one
        GTextNodeContainer *container = (GTextNodeContainer *)ctx->node;
        if (container->num_nodes > 0) {
          contexts[++level].node = container->nodes[0];
          continue;
        }
      } else {
        // This is a leaf node, draw using its draw method
        prv_base(ctx->node)->draw(ctx);
      }
    }

    // Descend levels if they are complete with this descent loop
    // We will continue to descend until we've either found a sibling or descended beyond level 0
    for (; level >= 0; level--) {
      GTextNodeDrawContext *ctx = &contexts[level];
      GTextNodeDrawContext *parent_ctx = (level > 0) ? &contexts[level - 1] : NULL;

      // We have entered here either by having completed drawing a node or by descending. If we
      // entered here by having completed drawing a node, we have either drawn a leaf node, an empty
      // container, or a cached node. If we entered here by descent, we have completed drawing a
      // container. If we have entered here by descent more than once, we have completed drawing a
      // container in a container.
      prv_iter_did_draw(ctx, parent_ctx);

      if (ctx->node->sibling) {
        // Continue this level to the next sibling
        contexts[level].node = ctx->node->sibling;
        break;
      }
    }
  }
  if (size_out) {
    *size_out = *contexts[0].size_out;
  }
  task_free(contexts);
}

static void prv_draw_text_node(GTextNode *node, GContext *ctx, const GRect *box,
                               const GTextNodeDrawConfig *config, bool render, GSize *size_out) {
  if (prv_should_draw_text_node(node, ctx, box, render)) {
    prv_draw_text_node_tree(node, ctx, box, config, render, size_out);
  } else if (size_out) {
    *size_out = node->cached_size;
  }
}

void graphics_text_node_get_size(GTextNode *node, GContext *ctx, const GRect *box,
                                 const GTextNodeDrawConfig *config, GSize *size_out) {
  const bool render = false;
  return prv_draw_text_node(node, ctx, box, config, render, size_out);
}

void graphics_text_node_draw(GTextNode *node, GContext *ctx, const GRect *box,
                             const GTextNodeDrawConfig *config, GSize *size_out) {
  const bool render = true;
  return prv_draw_text_node(node, ctx, box, config, render, size_out);
}
