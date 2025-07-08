/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "layout_node.h"

#include "timeline_layout.h"
#include "kernel/pbl_malloc.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "util/size.h"
#include "util/string.h"

static GTextNodeText *prv_create_text_node_buffer(const char *str) {
  const size_t str_length = str ? (strlen(str) + 1) : 0;
  GTextNodeText *text_node = graphics_text_node_create_text(str_length);
  if (text_node && str) {
    strncpy((char *)text_node->text, str, str_length);
  }
  return text_node;
}

static GTextNodeText *prv_create_text_node_attribute(const LayoutLayer *layout,
                                                     AttributeId attr_id) {
  const char *attr_str = attribute_get_string(layout->attributes, attr_id, "");
  if (IS_EMPTY_STRING(attr_str) && attr_id != AttributeIdUnused) {
    return NULL;
  }
  GTextNodeText *text_node = graphics_text_node_create_text(0);
  text_node->text = (char *)attr_str;
  return text_node;
}

static void prv_set_text_node_extent(GTextNode *node,
                                     const LayoutNodeExtentConfig *config) {
  // These are added instead of just set since in some cases, a node can have its extent influenced
  // twice. For example, a pre-configured node created by a Constructor Config with its own extent.
  node->offset.x += config->offset.x;
  node->offset.y += config->offset.y;
  node->margin.w += config->margin.w;
  node->margin.h += config->margin.h;
}

static const char *prv_get_font_key(const LayoutNodeTextConfig *config) {
  if (config->font_key) {
    return config->font_key;
  }
  const LayoutContentSize content_size = config->style;
  return (content_size == LayoutContentSize_Auto) ?
      system_theme_get_font_key(config->style_font) :
      system_theme_get_font_key_for_size(ToPreferredContentSize(content_size), config->style_font);
}

static void prv_set_text_node_text_parameters_from_config(
    GTextNodeText *text_node, const LayoutLayer *layout, const LayoutNodeTextConfig *config) {
  text_node->font = fonts_get_system_font(prv_get_font_key(config));
  const int fixed_lines = config->fixed_lines;
  const int line_spacing_delta = config->line_spacing_delta;
  text_node->max_size.h = fixed_lines ? (fixed_lines * (fonts_get_font_height(text_node->font) +
                                                        line_spacing_delta) - line_spacing_delta)
                                      : 0;
  text_node->overflow = GTextOverflowModeTrailingEllipsis;
  text_node->alignment = PBL_IF_RECT_ELSE(GTextAlignmentLeft, GTextAlignmentCenter);
  text_node->line_spacing_delta = line_spacing_delta;
  if (config->alignment) {
    text_node->alignment = ToGTextAlignment(config->alignment);
  }
#if PBL_COLOR
  const LayoutColors *colors = layout_get_colors(layout);
  switch (config->color) {
    case LayoutColor_None:
      break;
    case LayoutColor_Primary:
      text_node->color = colors->primary_color;
      break;
    case LayoutColor_Secondary:
      text_node->color = colors->secondary_color;
      break;
    case LayoutColor_Background:
      text_node->color = colors->bg_color;
      break;
  }
#endif
  prv_set_text_node_extent(&text_node->node, &config->extent);
}

static GTextNodeText *prv_create_text_attribute_node_from_config(
    const LayoutLayer *layout, const LayoutNodeTextAttributeConfig *config) {
  GTextNodeText *text_node = prv_create_text_node_attribute(layout, config->attr_id);
  if (text_node) {
    prv_set_text_node_text_parameters_from_config(text_node, layout, &config->text);
  }
  return text_node;
}

static GTextNodeText *prv_create_text_buffer_node_from_config(
    const LayoutLayer *layout, const LayoutNodeTextBufferConfig *config) {
  GTextNodeText *text_node = NULL;
  const char *str = config->use_i18n ? i18n_get(config->str, layout) : config->str;
  if (!IS_EMPTY_STRING(str)) {
    text_node = prv_create_text_node_buffer(str);
    prv_set_text_node_text_parameters_from_config(text_node, layout, &config->text);
  }
  if (config->use_i18n) {
    i18n_free(config->str, layout);
  }
  return text_node;
}

typedef struct {
  const LayoutLayer *layout;
  LayoutNodeTextDynamicConfig config;
  char buffer[];
} TextDynamicContext;

static void prv_text_dynamic_node_callback(GContext *ctx, GTextNode *node, const GRect *box,
                                           const GTextNodeDrawConfig *config, bool render,
                                           char *buffer, size_t buffer_size, void *user_data) {
  TextDynamicContext *text_context = user_data;
  text_context->config.update(text_context->layout, &text_context->config, buffer, render);
}

static GTextNodeTextDynamic *prv_create_text_dynamic_node_from_config(
    const LayoutLayer *layout, const LayoutNodeTextDynamicConfig *config) {
  PBL_ASSERTN(config);
  // Request a buffer sized to hold both a TextDynamicContext (used in the node's callback) as well
  // as the size requested by the provided LayoutNodeTextDynamicConfig
  GTextNodeTextDynamic *text_node = graphics_text_node_create_text_dynamic(
      sizeof(TextDynamicContext) + config->buffer_size, prv_text_dynamic_node_callback, NULL);
  if (text_node) {
    TextDynamicContext *context = (TextDynamicContext *)text_node->buffer;
    *context = (TextDynamicContext) {
      .layout = layout,
      .config = *config,
    };
    text_node->user_data = context;
    // graphics_text_node_create_text_dynamic() sets text_node->text.text to text_node->buffer,
    // but since we are using the start of that buffer for our TextDynamicContext we must override
    // text_node->text.text to the actual location of the text buffer here, if any
    if (config->buffer_size > 0) {
      // Zero out the first element of the buffer in case it's in the padding at the end of context
      // and there happens to be garbage there (which happens sometimes for clang in unit tests)
      ((char *)context->buffer)[0] = '\0';
      text_node->text.text = context->buffer;
    }
    prv_set_text_node_text_parameters_from_config(&text_node->text, layout, &config->text);
  }
  return text_node;
}

static GTextNodeText *prv_create_text_node_from_config(
    const LayoutLayer *layout, const LayoutNodeTextConfig *config) {
  GTextNodeText *text_node = prv_create_text_node_buffer(NULL);
  prv_set_text_node_text_parameters_from_config(text_node, layout, config);
  return text_node;
}

static void prv_setup_container_node_from_config(
    GTextNodeContainer *container_node, const LayoutLayer *layout,
    const LayoutNodeContainerConfig *config) {
  const uint16_t num_nodes = config->num_nodes;
  for (int i = 0; i < num_nodes; i++) {
    GTextNode *node = layout_create_text_node_from_config(layout, config->nodes[i]);
    graphics_text_node_container_add_child(container_node, node);
  }
  prv_set_text_node_extent(&container_node->node, &config->extent);
}

static GTextNodeHorizontal *prv_create_horizontal_container_node_from_config(
    const LayoutLayer *layout, const LayoutNodeHorizontalConfig *config) {
  const int capacity = config->container.num_nodes + config->container.extra_capacity;
  GTextNodeHorizontal *horizontal_node = graphics_text_node_create_horizontal(capacity);
  if (horizontal_node) {
    if (config->horizontal_alignment) {
      horizontal_node->horizontal_alignment = ToGTextAlignment(config->horizontal_alignment);
    }
    prv_setup_container_node_from_config(&horizontal_node->container, layout, &config->container);
  }
  return horizontal_node;
}

static GTextNodeVertical *prv_create_vertical_container_node_from_config(
    const LayoutLayer *layout, const LayoutNodeVerticalConfig *config) {
  const int capacity = config->container.num_nodes + config->container.extra_capacity;
  GTextNodeVertical *vertical_node = graphics_text_node_create_vertical(capacity);
  if (vertical_node) {
    if (config->vertical_alignment) {
      vertical_node->vertical_alignment = ToGVerticalAlignment(config->vertical_alignment);
    }
    prv_setup_container_node_from_config(&vertical_node->container, layout, &config->container);
  }
  return vertical_node;
}

static GTextNode *prv_create_node_from_constructor_config(
    const LayoutLayer *layout, const LayoutNodeConstructorConfig *config) {
  GTextNode *node = config->constructor(layout, config);
  if (node) {
    prv_set_text_node_extent(node, &config->extent);
  }
  return node;
}

static GTextNode *prv_create_timeline_icon_node_from_config(
    const LayoutLayer *layout, const LayoutNodeExtentConfig *config) {
  GTextNode *node = &timeline_layout_create_icon_node((const TimelineLayout *)layout)->node;
  prv_set_text_node_extent(node, config);
  return node;
}

static const char *prv_get_font_key_for_size(TextStyleFont style_font,
                                             LayoutContentSize content_size) {
  return (content_size == LayoutContentSize_Auto) ?
      system_theme_get_font_key(style_font) :
      system_theme_get_font_key_for_size(ToPreferredContentSize(content_size), style_font);
}

GTextNodeVertical *layout_create_headings_paragraphs_node(
    const LayoutLayer *layout, const LayoutNodeHeadingsParagraphsConfig *config) {
  const AttributeList *attributes = layout->attributes;
  StringList *headings = attribute_get_string_list(attributes, AttributeIdHeadings);
  StringList *paragraphs = attribute_get_string_list(attributes, AttributeIdParagraphs);
  const size_t num_headings = string_list_count(headings);
  if (num_headings == 0) {
    return NULL;
  }

  const LayoutNodeTextConfig s_heading_config = {
    .extent.node.type = LayoutNodeType_Text,
    .font_key = prv_get_font_key_for_size(config->heading_style_font, config->size),
  };
  const LayoutNodeTextConfig s_paragraph_config = {
    .extent.node.type = LayoutNodeType_Text,
    .font_key = prv_get_font_key_for_size(config->paragraph_style_font, config->size),
    .line_spacing_delta = -2,
    .extent.margin.h = 17,
  };

  GTextNodeVertical *vertical_node = graphics_text_node_create_vertical(num_headings * 2);

  for (unsigned int i = 0; i < num_headings; i++) {
    const char *heading = string_list_get_at(headings, i);
    const char *paragraph = string_list_get_at(paragraphs, i);
    if (!heading || !paragraph) {
      break;
    }

    GTextNodeText *heading_node =
        (GTextNodeText *)layout_create_text_node_from_config(
            layout, &s_heading_config.extent.node);
    GTextNodeText *paragraph_node =
        (GTextNodeText *)layout_create_text_node_from_config(
            layout, &s_paragraph_config.extent.node);
    heading_node->text = (char *)heading;
    paragraph_node->text = (char *)paragraph;
    graphics_text_node_container_add_child(&vertical_node->container, &heading_node->node);
    graphics_text_node_container_add_child(&vertical_node->container, &paragraph_node->node);
  }

  return vertical_node;
}

static GTextNode *prv_create_headings_paragraphs_node(
    const LayoutLayer *layout, const LayoutNodeHeadingsParagraphsConfig *config) {
  GTextNode *node = &layout_create_headings_paragraphs_node(layout, config)->container.node;
  if (node) {
    prv_set_text_node_extent(node, (LayoutNodeExtentConfig *)config);
  }
  return node;
}

#define METRICS_PER_PAGE 2

typedef struct {
  const char *name;
  const char *value;
  AppResourceInfo *icon_info;
  KinoLayer *icon_layer;
  int index;
} MetricContext;

static GTextNode *prv_metric_constructor(
    const LayoutLayer *layout, const LayoutNodeConstructorConfig *config) {
  MetricContext *context = config->context;
  const int icon_offset_x = PBL_IF_RECT_ELSE(-2, 0);
  const int icon_offset_y = PBL_IF_RECT_ELSE(4, 0);
  const int icon_margin_w = PBL_IF_RECT_ELSE(3, 0);
  const int icon_margin_h = PBL_IF_RECT_ELSE(0, -1);
  const LayoutNodeExtentConfig timeline_icon_config = {
    .node.type = LayoutNodeType_TimelineIcon,
    .offset.x = icon_offset_x,
    .offset.y = icon_offset_y,
    .margin.w = icon_margin_w,
    .margin.h = icon_margin_h,
  };
  const LayoutNodeIconConfig icon_config = {
    .extent.node.type = LayoutNodeType_Icon,
    .res_info = context->icon_info,
    .icon_layer = &context->icon_layer,
    .align = PBL_IF_ROUND_ELSE(GAlignCenter, GAlignLeft),
    .extent.offset.x = icon_offset_x,
    .extent.offset.y = icon_offset_y,
    .extent.margin.w = icon_margin_w,
    .extent.margin.h = icon_margin_h,
  };
  const LayoutNodeTextBufferConfig name_config = {
    .text.extent.node.type = LayoutNodeType_TextBuffer,
    .str = context->name,
    .text.font_key = FONT_KEY_GOTHIC_14,
    .text.extent.margin.h = -1 // name margin height
  };
  const LayoutNodeTextBufferConfig value_config = {
    .text.extent.node.type = LayoutNodeType_TextBuffer,
    .str = context->value,
    .text.font_key = FONT_KEY_GOTHIC_18_BOLD,
  };
  const LayoutNodeConfig * const icon_config_node =
      (context->index == 0) ? &timeline_icon_config.node :
                              &icon_config.extent.node;
  const LayoutNodeConfig * const vertical_config_nodes[] = {
    PBL_IF_ROUND_ELSE(icon_config_node, NULL),
    &name_config.text.extent.node,
    &value_config.text.extent.node,
  };
  const LayoutNodeVerticalConfig vertical_config = {
    .container.extent.node.type = LayoutNodeType_Vertical,
    .container.num_nodes = ARRAY_LENGTH(vertical_config_nodes),
    .container.nodes = (LayoutNodeConfig **)&vertical_config_nodes,
  };
#if PBL_RECT
  const LayoutNodeConfig * const horizontal_config_nodes[] = {
    icon_config_node,
    &vertical_config.container.extent.node,
  };
  const LayoutNodeHorizontalConfig horizontal_config = {
    .container.extent.node.type = LayoutNodeType_Horizontal,
    .container.num_nodes = ARRAY_LENGTH(horizontal_config_nodes),
    .container.nodes = (LayoutNodeConfig **)&horizontal_config_nodes,
  };
#endif
  GTextNode *metric_node = (GTextNode *)layout_create_text_node_from_config(
      layout, PBL_IF_ROUND_ELSE(&vertical_config.container.extent.node,
                                &horizontal_config.container.extent.node));
  if (context->index > 0) {
    PBL_UNUSED const int metric_margin_top = PBL_IF_RECT_ELSE(0, 5);
    PBL_UNUSED const int metric_margin_h_rect = 14;
    PBL_UNUSED const int metric_margin_h_round_inner = 20;
    PBL_UNUSED const int metric_margin_h_round_page = 24;
    const int metric_margin_h =
        PBL_IF_RECT_ELSE(metric_margin_h_rect,
                         // special case for the first metric after a page break
                         (context->index == METRICS_PER_PAGE) ? metric_margin_top :
                         (context->index % METRICS_PER_PAGE) ? metric_margin_h_round_inner :
                                                               metric_margin_h_round_page);
    metric_node->offset.y += metric_margin_h;
    metric_node->margin.h += metric_margin_h;
  }
  return metric_node;
}

//! Adds a metric node to a vertically and automatically adds a page break on round
static void prv_add_metric(TimelineLayout *layout, GTextNodeVertical *vertical_node, int index,
                           GTextNode *metric_node) {
  graphics_text_node_container_add_child(&vertical_node->container, metric_node);
#if PBL_ROUND
  if (index == METRICS_PER_PAGE - 1 && !layout->has_page_break) {
    // after filling a page with metric nodes, add a page break
    layout->has_page_break = true;
    GTextNode *page_break =
        &timeline_layout_create_page_break_node((const TimelineLayout *)layout)->node;
    graphics_text_node_container_add_child(&vertical_node->container, page_break);
  }
#endif
}

GTextNodeVertical *layout_create_metrics_node(const LayoutLayer *layout_ref) {
  // TODO: Remove TimelineLayout requirement
  TimelineLayout *layout = (TimelineLayout *)layout_ref;
  if (layout->metric_icon_layers) {
    return NULL;
  }

  const AttributeList *attributes = layout_ref->attributes;
  StringList *names = attribute_get_string_list(attributes, AttributeIdMetricNames);
  StringList *values = attribute_get_string_list(attributes, AttributeIdMetricValues);
  Uint32List *icons = attribute_get_uint32_list(attributes, AttributeIdMetricIcons);
  if (!icons) {
    return NULL;
  }

  // String list access is out-of-bounds safe, so use the Uint32List num_values
  const size_t num_metrics = icons->num_values;
  if (!num_metrics) {
    return NULL;
  }

  if (num_metrics > 1) {
    layout->num_metric_icon_layers = num_metrics - 1;
    layout->metric_icon_layers = task_zalloc_check((num_metrics - 1) * sizeof(KinoLayer *));
  }

  const int num_nodes = PBL_IF_ROUND_ELSE(num_metrics + 1, // optional page break
                                          num_metrics);
  GTextNodeVertical *vertical_node = graphics_text_node_create_vertical(num_nodes);

  for (unsigned int i = 0; i < num_metrics; i++) {
    const char *name = string_list_get_at(names, i);
    const char *value = string_list_get_at(values, i);
    if (!name || !value) {
      break;
    }

    TimelineResourceInfo icon_info = {
      .res_id = icons->values[i],
      .app_id = &layout->info->app_id,
    };
    AppResourceInfo icon_res_info;
    timeline_resources_get_id(&icon_info, TimelineResourceSizeTiny, &icon_res_info);

    const LayoutNodeConstructorConfig metric_config = {
      .extent.node.type = LayoutNodeType_Constructor,
      .constructor = prv_metric_constructor,
      .context = &(MetricContext) {
        .index = i,
        .name = name,
        .value = value,
        .icon_info = &icon_res_info,
        .icon_layer = i == 0 ? &layout->icon_layer :
                               layout->metric_icon_layers[i - 1],
      },
    };

    GTextNodeText *metric_node =
        (GTextNodeText *)layout_create_text_node_from_config(
            layout_ref, &metric_config.extent.node);
    prv_add_metric(layout, vertical_node, i, &metric_node->node);
  }

  return vertical_node;
}

static GTextNode *prv_create_metrics_node(const LayoutLayer *layout,
                                          const LayoutNodeConfig *config) {
  GTextNode *node = &layout_create_metrics_node(layout)->container.node;
  prv_set_text_node_extent(node, (LayoutNodeExtentConfig *)config);
  return node;
}

static GTextNode *prv_create_icon_node_from_config(const LayoutLayer *layout,
                                                   const LayoutNodeIconConfig *config) {
  KinoReel *icon_reel = kino_reel_create_with_resource_system(config->res_info->res_app_num,
                                                              config->res_info->res_id);
  if (!icon_reel) {
    return NULL;
  }

  KinoLayer *icon_layer = kino_layer_create((GRect) { .size = kino_reel_get_size(icon_reel) });
  *config->icon_layer = icon_layer;
  kino_layer_set_alignment(icon_layer, config->align);
  kino_layer_set_reel(icon_layer, icon_reel, true /* take_ownership */);
  layer_add_child((void *)&layout->layer, &icon_layer->layer);
  GTextNodeCustom *custom = layout_node_create_kino_layer_wrapper(icon_layer);
  prv_set_text_node_extent(&custom->node, &config->extent);
  return &custom->node;
}

GTextNode *layout_create_text_node_from_config(const LayoutLayer *layout,
                                               const LayoutNodeConfig *config) {
  if (!config) {
    return NULL;
  }
  switch (config->type) {
    case LayoutNodeType_TextAttribute:
      return &prv_create_text_attribute_node_from_config(
          layout, (LayoutNodeTextAttributeConfig *)config)->node;
    case LayoutNodeType_TextBuffer:
      return &prv_create_text_buffer_node_from_config(
          layout, (LayoutNodeTextBufferConfig *)config)->node;
    case LayoutNodeType_TextDynamic:
      return &prv_create_text_dynamic_node_from_config(
          layout, (LayoutNodeTextDynamicConfig *)config)->text.node;
    case LayoutNodeType_Text:
      return &prv_create_text_node_from_config(layout, (LayoutNodeTextConfig *)config)->node;
    case LayoutNodeType_Horizontal:
      return &prv_create_horizontal_container_node_from_config(
          layout, (LayoutNodeHorizontalConfig *)config)->container.node;
    case LayoutNodeType_Vertical:
      return &prv_create_vertical_container_node_from_config(
          layout, (LayoutNodeVerticalConfig *)config)->container.node;
    case LayoutNodeType_Constructor:
      return prv_create_node_from_constructor_config(layout, (LayoutNodeConstructorConfig *)config);
    case LayoutNodeType_Icon:
#if !PLATFORM_TINTIN
      return prv_create_icon_node_from_config(layout, (LayoutNodeIconConfig *)config);
#else
      return NULL;
#endif
    case LayoutNodeType_TimelineIcon:
      return prv_create_timeline_icon_node_from_config(layout, (LayoutNodeExtentConfig *)config);
    case LayoutNodeType_TimelinePageBreak:
      return &timeline_layout_create_page_break_node((const TimelineLayout *)layout)->node;
    case LayoutNodeType_TimelineMetrics:
#if !PLATFORM_TINTIN
      return prv_create_metrics_node(layout, config);
#else
      return NULL;
#endif
    case LayoutNodeType_HeadingsParagraphs:
      return prv_create_headings_paragraphs_node(
          layout, (LayoutNodeHeadingsParagraphsConfig *)config);
  }
  return NULL;
}

static void prv_kino_layer_wrapper_callback(GContext *ctx, const GRect *box,
                                            const GTextNodeDrawConfig *config, bool render,
                                            GSize *size_out, void *user_data) {
  KinoLayer *kino_layer = user_data;
  GRect frame = kino_layer->layer.frame;
  if (!render) {
    grect_align(&frame, box, kino_layer_get_alignment(kino_layer), false);
    frame.origin.y = box->origin.y;
    layer_set_frame(&kino_layer->layer, &frame);
  }
  if (size_out) {
    *size_out = frame.size;
  }
}

GTextNodeCustom *layout_node_create_kino_layer_wrapper(KinoLayer *kino_layer) {
  return graphics_text_node_create_custom(prv_kino_layer_wrapper_callback, (void *)kino_layer);
}
